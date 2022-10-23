
#include <iostream>


void setLogLevel(uint16_t newLoglevel);


void useColormap(int newTypeColormap);


void useLepton(int newTypeLepton) ;

void useSpiSpeedMhz(unsigned int newSpiSpeed);

void setAutomaticScalingRange();

void useRangeMinValue(uint16_t newMinValue) ;

void useRangeMaxValue(uint16_t newMaxValue);

void run() ;

void performFFC() ;

void log_message(uint16_t level, std::string msg) ;
