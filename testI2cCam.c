
#include "stdio.h"
#include "stdlib.h"
#include <stdint.h>
#include "leptonSDKEmb32PUB/LEPTON_I2C_Service.h"
int main()
{
    if(LEP_I2C_MasterOpen(1 , 100000) != LEP_OK)
    {
        printf("Error Openning\r\n");
    }

    uint16_t reg ;
    if(LEP_I2C_MasterReadRegister(1 , 0x2A , 0x02,&reg )  != LEP_OK)
    {
        printf("Error read reg\r\n") ;
    }
    printf("reg %x\r\n" , reg);

}
