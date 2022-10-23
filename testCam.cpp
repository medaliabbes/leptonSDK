
#include <iostream>
#include "stdio.h"
#include "stdlib.h"
#include "Lepton_I2C.h"
#include "leptonFun.h"

int main()
{

    int typeColormap = 3; // colormap_ironblack
	int typeLepton = 3; // Lepton 2.x
	int spiSpeed = 20; // SPI bus speed 20MHz
	int rangeMin = -1; //
	int rangeMax = -1; //
	int loglevel = 100 ;
    
     //lepton_connect() ;
    // lepton_perform_ffc() ;
    //LeptonThread *thread = new LeptonThread();
	setLogLevel(loglevel);
	printf("LOG\r\n");
	useColormap(typeColormap);
	printf("setColor\r\n");
	useLepton(typeLepton);
	printf("use Lepton\r\n");
	useSpiSpeedMhz(spiSpeed);
	setAutomaticScalingRange();
	
    run() ;

    return 0 ;
}

