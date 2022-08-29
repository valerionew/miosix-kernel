
#include <cstdio>
#include "miosix.h"
#include "WS2812.h"
#include <RGB.h>

#define numleds 60
using namespace std;
using namespace miosix;


using sck  = Gpio<GPIOB_BASE,13>; //Used as HW SPI
using mosi = Gpio<GPIOB_BASE,15>; //Used as HW SPI
using miso = Gpio<GPIOB_BASE,14>; //Used as HW SPI

static void spi2sendOnly(unsigned char x){
    //NOTE: data is sent after the function returns, watch out!
    while((SPI2->SR & SPI_SR_TXE)==0) ;
    SPI2->DR=x;
}

void spi_init(){
    {
        FastInterruptDisableLock dLock;
        sck::mode(Mode::ALTERNATE);  //sck::alternateFunction(5);
        mosi::mode(Mode::ALTERNATE); //mosi::alternateFunction(5);
        miso::mode(Mode::ALTERNATE); //miso::alternateFunction(5);
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
        RCC_SYNC();
    }

        SPI2->CR1=SPI_CR1_SSM  //No HW cs
                | SPI_CR1_SSI
                | SPI_CR1_SPE  //SPI enable 
                | SPI_CR1_MSTR;//Master mode
}

void spi_transmit(uint8_t * data, uint32_t length){
    uint8_t *pointer = data;
    uint32_t remaining = length;
    while(length > 0){
        spi2sendOnly(*pointer);
        while(SPI2->SR & SPI_SR_BSY) ;
        length--;
        pointer++;
    }
}

const RGB_t<uint8_t>	violet	( 75,   0, 130);
const RGB_t<uint8_t>	blue	(  0,   0, 255);
const RGB_t<uint8_t>	green	(  0, 255,   0);
const RGB_t<uint8_t>	yellow	(127, 255,   0);
const RGB_t<uint8_t>	orange	(255,  50,   0);
const RGB_t<uint8_t>	red		(255,   0,   0);

std::array<RGB_t<uint8_t>, 6> rainbow = {violet, blue, green, yellow, orange, red};

WS2812<numleds> leds(spi_transmit);


int main(){  
    iprintf("entry\n");  
    spi_init();
    iprintf("spi init done\n");        
    while(1){
        for(int i = 6; i >0 ; i--){
            for(int j = 0; j < numleds; j++){
                leds.setPixel(j, rainbow[(j+i)%6]);
            }
        leds.show();

        Thread::sleep(50);
		}

    }
}

