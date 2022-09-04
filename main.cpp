#include <cstdio>
#include "miosix.h"
#include "WS2812.h"
#include <RGB.h>
#include <kernel/scheduler/scheduler.h>
#include <interfaces/delays.h>


#define numleds 60

using namespace miosix;

static Thread *waiting;

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

using sck  = Gpio<GPIOB_BASE,13>; //Used as HW SPI
using mosi = Gpio<GPIOB_BASE,15>; //Used as HW SPI
using miso = Gpio<GPIOB_BASE,14>; //Used as HW SPI
using sig = Gpio<GPIOB_BASE,3>; 


void spi_dma_init(){
        // using DMA1 channel 4 and SPI2
        FastInterruptDisableLock dLock;

        sck::mode(Mode::ALTERNATE);   
        sck::alternateFunction(5);
        mosi::mode(Mode::ALTERNATE); 
        mosi::alternateFunction(5);
        miso::mode(Mode::ALTERNATE); 
        miso::alternateFunction(5);
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; // enable DMA1 clock
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; // enable SPI2 clock

        RCC_SYNC();
        SPI2->CR2=SPI_CR2_TXDMAEN;
        SPI2->CR1=SPI_CR1_SSM  //software slave management
                | SPI_CR1_SSI  //software slave set
                | SPI_CR1_BR_0 //fclk/4 works fine
                | SPI_CR1_SPE  //SPI enable 
                | SPI_CR1_MSTR;//Master mode

}

void spi_transmit_dma(uint8_t * data, uint32_t length){

        pthread_mutex_lock(&mutex);
        waiting=Thread::getCurrentThread();

        NVIC_ClearPendingIRQ(DMA1_Stream4_IRQn);
        NVIC_SetPriority(DMA1_Stream4_IRQn,10);//Low priority for DMA
        NVIC_EnableIRQ(DMA1_Stream4_IRQn);

        DMA1_Stream4->CR=0;
        DMA1_Stream4->PAR=reinterpret_cast<unsigned int>(&SPI2->DR); //Peripheral address
        DMA1_Stream4->M0AR=reinterpret_cast<unsigned int>(data); // data address
        DMA1_Stream4->NDTR=length; // transfer size
        DMA1_Stream4->CR=DMA_SxCR_PL_1 //High priority because fifo disabled
                    | DMA_SxCR_MINC    //Increment memory pointer
                    | DMA_SxCR_DIR_0   //Memory to peripheral
                    | DMA_SxCR_TCIE    //Interrupt on transfer complete
                    | DMA_SxCR_TEIE    //Interrupt on transfer error
                    | DMA_SxCR_DMEIE   //Interrupt on direct mode error
                    | DMA_SxCR_EN;     // start DMA
                        
    {
        FastInterruptDisableLock dLock;
        while(waiting!=0)
        {
            waiting->IRQwait();
            {
                FastInterruptEnableLock eLock(dLock);
                Thread::yield();
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}


void __attribute__((naked)) DMA1_Stream4_IRQHandler()
{
    saveContext();
        DMA1->HIFCR=DMA_HIFCR_CTCIF4
              | DMA_HIFCR_CTEIF4
              | DMA_HIFCR_CDMEIF4;
    waiting->IRQwakeup();
    if(waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
    Scheduler::IRQfindNextThread();
    waiting=0;
    restoreContext();
}


const RGB_t<uint8_t>	violet	( 75,   0, 130);
const RGB_t<uint8_t>	blue	(  0,   0, 255);
const RGB_t<uint8_t>	green	(  0, 255,   0);
const RGB_t<uint8_t>	yellow	(127, 255,   0);
const RGB_t<uint8_t>	orange	(255,  50,   0);
const RGB_t<uint8_t>	red		(255,   0,   0);
const RGB_t<uint8_t>	white	(255, 255, 255);
const RGB_t<uint8_t>	black	(  0,   0,   0);

std::array<RGB_t<uint8_t>, 6> rainbow = {violet, blue, green, yellow, orange, red};

WS2812<numleds> leds(spi_transmit_dma);


int main(){  
    sig::mode(Mode::OUTPUT);
    iprintf("entry\n");  
    spi_dma_init();
    iprintf("spi init done\n");    
        ledOn();    
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

