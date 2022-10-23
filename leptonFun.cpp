#include <iostream>
#include "Palettes.h"
#include "SPI.h"
#include "Lepton_I2C.h"
#include "leptonFun.h"
#include <cstring>
#include <climits>

#define PACKET_SIZE 164
#define PACKET_SIZE_UINT16 (PACKET_SIZE/2)
#define PACKETS_PER_FRAME 30
#define FRAME_SIZE_UINT16 (PACKET_SIZE_UINT16*PACKETS_PER_FRAME)
#define FPS 1;

uint16_t loglevel;
  int typeColormap;
  const int *selectedColormap;
  int selectedColormapSize;
  int typeLepton;
  unsigned int spiSpeed;
  bool autoRangeMin;
  bool autoRangeMax;
  uint16_t rangeMin;
  uint16_t rangeMax;
  int myImageWidth;
  int myImageHeight;
  uint8_t result[PACKET_SIZE*PACKETS_PER_FRAME];
  uint8_t shelf[4][PACKET_SIZE*PACKETS_PER_FRAME];
  uint16_t *frameBuffer;


static unsigned int lepton_image[120][160];
static void save_pgm_file(void)
{
    int i;
    int j;
    unsigned int maxval = 0;
    unsigned int minval = UINT_MAX;
    char image_name[32];
    int image_index = 0;

    do {
        sprintf(image_name, "images/IMAG_%.4d.pgm", image_index);
        image_index += 1;
        if (image_index > 9999) 
        {
            image_index = 0;
            break;
        }

    } while (access(image_name, F_OK) == 0);

    FILE *f = fopen(image_name, "w+");
    if (f == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }
    
    printf("Calculating min/max values for proper scaling...\n");
    for(i = 0; i < 120; i++)
    {
        
        for(j = 0; j < 160; j++)
        {
            if (lepton_image[i][j] > maxval) {
                maxval = lepton_image[i][j];
            }
            if (lepton_image[i][j] < minval) {
                minval = lepton_image[i][j];
            }
        }
    }
    printf("maxval = %u\n",maxval);
    printf("minval = %u\n",minval);
    
    fprintf(f,"P2\n160 120\n%u\n",maxval-minval);
    for(i=0; i < 120; i +=1 )
    {
        /* first 80 pixels in row */
        for(j = 0; j < 160; j++)
        {
            fprintf(f,"%d ", lepton_image[i][j] - minval);
        }
	fprintf(f,"\n");
    }
    fprintf(f,"\n\n");

    fclose(f);

    //launch image viewer
    //execlp("gpicview", image_name, NULL);
}


void setLogLevel(uint16_t newLoglevel)
{
	loglevel = newLoglevel;
}

void useColormap(int newTypeColormap)
{
	switch (newTypeColormap) {
	case 1:
		typeColormap = 1;
		selectedColormap = colormap_rainbow;
		selectedColormapSize = get_size_colormap_rainbow();
		break;
	case 2:
		typeColormap = 2;
		selectedColormap = colormap_grayscale;
		selectedColormapSize = get_size_colormap_grayscale();
		break;
	default:
		typeColormap = 3;
		selectedColormap = colormap_ironblack;
		selectedColormapSize = get_size_colormap_ironblack();
		break;
	}
}

void useLepton(int newTypeLepton)
{
	switch (newTypeLepton) {
	case 3:
		typeLepton = 3;
		myImageWidth = 160;
		myImageHeight = 120;
		break;
	default:
		typeLepton = 2;
		myImageWidth = 80;
		myImageHeight = 60;
	}
}

void useSpiSpeedMhz(unsigned int newSpiSpeed)
{
	spiSpeed = newSpiSpeed * 1000 * 1000;
}

void setAutomaticScalingRange()
{
	autoRangeMin = true;
	autoRangeMax = true;
}

void useRangeMinValue(uint16_t newMinValue)
{
	autoRangeMin = false;
	rangeMin = newMinValue;
}

void useRangeMaxValue(uint16_t newMaxValue)
{
	autoRangeMax = false;
	rangeMax = newMaxValue;
}

void run()
{
	//create the initial image
	//myImage = QImage(myImageWidth, myImageHeight, QImage::Format_RGB888);

	const int *colormap = selectedColormap;
	const int colormapSize = selectedColormapSize;
	uint16_t minValue = rangeMin;
	uint16_t maxValue = rangeMax;
	float diff = maxValue - minValue;
	float scale = 255/diff;
	uint16_t n_wrong_segment = 0;
	uint16_t n_zero_value_drop_frame = 0;

	//open spi port
	SpiOpenPort(0, spiSpeed);

	while(true) {

		//read data packets from lepton over SPI
		int resets = 0;
		int segmentNumber = -1;
		for(int j=0;j<PACKETS_PER_FRAME;j++) {
			//if it's a drop packet, reset j to 0, set to -1 so he'll be at 0 again loop
			read(spi_cs0_fd, result+sizeof(uint8_t)*PACKET_SIZE*j, sizeof(uint8_t)*PACKET_SIZE);
			int packetNumber = result[j*PACKET_SIZE+1];
			if(packetNumber != j) {
				j = -1;
				resets += 1;
				usleep(1000);
				//Note: we've selected 750 resets as an arbitrary limit, since there should never be 750 "null" packets between two valid transmissions at the current poll rate
				//By polling faster, developers may easily exceed this count, and the down period between frames may then be flagged as a loss of sync
				if(resets == 750) {
					SpiClosePort(0);
					lepton_reboot();
					n_wrong_segment = 0;
					n_zero_value_drop_frame = 0;
					usleep(750000);
					SpiOpenPort(0, spiSpeed);
				}
				continue;
			}
			if ((typeLepton == 3) && (packetNumber == 20)) {
				segmentNumber = (result[j*PACKET_SIZE] >> 4) & 0x0f;
				if ((segmentNumber < 1) || (4 < segmentNumber)) {
					log_message(10, "[ERROR] Wrong segment number " + std::to_string(segmentNumber));
					break;
				}
			}
		}
		if(resets >= 30) {
			log_message(3, "done reading, resets: " + std::to_string(resets));
		}


		//
		int iSegmentStart = 1;
		int iSegmentStop;
		if (typeLepton == 3) {
			if ((segmentNumber < 1) || (4 < segmentNumber)) {
				n_wrong_segment++;
				if ((n_wrong_segment % 12) == 0) {
					log_message(5, "[WARNING] Got wrong segment number continuously " + std::to_string(n_wrong_segment) + " times");
				}
				continue;
			}
			if (n_wrong_segment != 0) {
				log_message(8, "[WARNING] Got wrong segment number continuously " + std::to_string(n_wrong_segment) + " times [RECOVERED] : " + std::to_string(segmentNumber));
				n_wrong_segment = 0;
			}

			//
			memcpy(shelf[segmentNumber - 1], result, sizeof(uint8_t) * PACKET_SIZE*PACKETS_PER_FRAME);
			if (segmentNumber != 4) {
				continue;
			}
			iSegmentStop = 4;
		}
		else {
			memcpy(shelf[0], result, sizeof(uint8_t) * PACKET_SIZE*PACKETS_PER_FRAME);
			iSegmentStop = 1;
		}

		if ((autoRangeMin == true) || (autoRangeMax == true)) {
			if (autoRangeMin == true) {
				maxValue = 65535;
			}
			if (autoRangeMax == true) {
				maxValue = 0;
			}
			for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {
				for(int i=0;i<FRAME_SIZE_UINT16;i++) {
					//skip the first 2 uint16_t's of every packet, they're 4 header bytes
					if(i % PACKET_SIZE_UINT16 < 2) {
						continue;
					}

					//flip the MSB and LSB at the last second
					uint16_t value = (shelf[iSegment - 1][i*2] << 8) + shelf[iSegment - 1][i*2+1];
					if (value == 0) {
						// Why this value is 0?
						continue;
					}
					if ((autoRangeMax == true) && (value > maxValue)) {
						maxValue = value;
					}
					if ((autoRangeMin == true) && (value < minValue)) {
						minValue = value;
					}
				}
			}
			diff = maxValue - minValue;
			scale = 255/diff;
		}
	        int counter = 0 ;
		int row, column;
		uint16_t value;
		uint16_t valueFrameBuffer;
		//QRgb color;
		for(int iSegment = iSegmentStart; iSegment <= iSegmentStop; iSegment++) {
			int ofsRow = 30 * (iSegment - 1);
			for(int i=0;i<FRAME_SIZE_UINT16;i++) {
				//skip the first 2 uint16_t's of every packet, they're 4 header bytes
				if(i % PACKET_SIZE_UINT16 < 2) {
					continue;
				}

				//flip the MSB and LSB at the last second
				valueFrameBuffer = (shelf[iSegment - 1][i*2] << 8) + shelf[iSegment - 1][i*2+1];
				if (valueFrameBuffer == 0) {
					// Why this value is 0?
					n_zero_value_drop_frame++;
					if ((n_zero_value_drop_frame % 12) == 0) {
						log_message(5, "[WARNING] Found zero-value. Drop the frame continuously " + std::to_string(n_zero_value_drop_frame) + " times");
					}
					break;
				}

				//
				value = (valueFrameBuffer - minValue) * scale;
				int ofs_r = 3 * value + 0; if (colormapSize <= ofs_r) ofs_r = colormapSize - 1;
				int ofs_g = 3 * value + 1; if (colormapSize <= ofs_g) ofs_g = colormapSize - 1;
				int ofs_b = 3 * value + 2; if (colormapSize <= ofs_b) ofs_b = colormapSize - 1;
				//color = qRgb(colormap[ofs_r], colormap[ofs_g], colormap[ofs_b]);
				//if (typeLepton == 3) {
					column = (i % PACKET_SIZE_UINT16) - 2 + (myImageWidth / 2) * ((i % (PACKET_SIZE_UINT16 * 2)) / PACKET_SIZE_UINT16);
					row = i / PACKET_SIZE_UINT16 / 2 + ofsRow;
				//}
				/*else {
					column = (i % PACKET_SIZE_UINT16) - 2;
					row = i / PACKET_SIZE_UINT16;
				}*/
				if(valueFrameBuffer == 0)
					{
					printf("ZEro in frame\r\n");
					 while(1);}
				lepton_image[row][column]
				 = (ofs_r + ofs_g +ofs_b)/3;
				counter++;
                               // printf("%d" ,row);
				//printf("%d,",column);
				//myImage.setPixel(column, row, color);
			}
		         printf("\r\n");
		}

		if (n_zero_value_drop_frame != 0) {
			log_message(8, "[WARNING] Found zero-value. Drop the frame continuously " + std::to_string(n_zero_value_drop_frame) + " times [RECOVERED]");
			n_zero_value_drop_frame = 0;
		}

		//lets emit the signal for update
		//emit updateImage(myImage);
		save_pgm_file();                
		printf("col %d,row %d,coun %d\r\n",column ,row, counter);
	}
	
	//finally, close SPI port just bcuz
	SpiClosePort(0);
}

void performFFC() {
	//perform FFC
	lepton_perform_ffc();
}

void log_message(uint16_t level, std::string msg)
{
	if (level <= loglevel) {
		std::cerr << msg << std::endl;
	}
}

