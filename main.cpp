#include <cstdio>
#include "miosix.h"
#include "WS2812.h"
#include <RGB.h>
#include <RGBColors.h>
#include <kernel/scheduler/scheduler.h>
#include <interfaces/delays.h>


#define numleds 60

using namespace miosix;

static Thread *waiting;

static bool dmaerror = false;


Mutex mutex;
ConditionVariable full;

//using sck  = Gpio<GPIOB_BASE,13>; // we don't even need to occupy a pin for the clock, just the mosi is enough
using WS2812pin = Gpio<GPIOB_BASE,15>; //Used as HW SPI


void spi_dma_init(){
        // using DMA1 channel 4 and SPI2
        FastInterruptDisableLock dLock;

        //sck::mode(Mode::ALTERNATE); // not needed   
        //sck::alternateFunction(5);
        WS2812pin::mode(Mode::ALTERNATE); 
        WS2812pin::alternateFunction(5);

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

bool spi_transmit_dma(uint8_t * data, uint32_t length){

        waiting=Thread::getCurrentThread();
        dmaerror =false; //clear prev errors

        NVIC_ClearPendingIRQ(DMA1_Stream4_IRQn); // clear prev interrupts if pending
        NVIC_SetPriority(DMA1_Stream4_IRQn,10);//Low priority for DMA
        NVIC_EnableIRQ(DMA1_Stream4_IRQn); // enable DMA interrupt 

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
    bool result=!dmaerror;
    return result;
}

void __attribute__((used)) SPI2txDmaHandlerImpl()
{
    if(DMA1->HISR & (DMA_HISR_TEIF4 | DMA_HISR_DMEIF4)) dmaerror=true;
    DMA1->HIFCR=DMA_HIFCR_CTCIF4
              | DMA_HIFCR_CTEIF4
              | DMA_HIFCR_CDMEIF4;
    waiting->IRQwakeup();
    if(waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();
    waiting=0;
}

/**
 * DMA TX end of transfer
 */
void __attribute__((naked)) DMA1_Stream4_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z20SPI2txDmaHandlerImplv");
    restoreContext();
}

using namespace RGBColors;

std::array<RGB_t<uint8_t>, 6> rainbow = {violet, blue, green, yellow, orange, red};

std::array<RGB_t<uint8_t>, 60> long_rainbow ={
    violet, violet, violet, violet, violet, 
    violet, violet, violet, violet, violet,
    blue,   blue,   blue,   blue,   blue, 
    blue,   blue,   blue,   blue,   blue,
    green,  green,  green,  green,  green, 
    green,  green,  green,  green,  green,
    yellow, yellow, yellow, yellow, yellow, 
    yellow, yellow, yellow, yellow, yellow,
    orange, orange, orange, orange, orange, 
    orange, orange, orange, orange, orange,
    red,    red,    red,    red,    red, 
    red,    red,    red,    red,    red
};

std::array<RGB_t<uint8_t>, 60> dot ={
    white, black, black, black, black, black, black, black, black, black,
    black, black, black, black, black, black, black, black, black, black,
    black, black, black, black, black, black, black, black, black, black,
    black, black, black, black, black, black, black, black, black, black,
    black, black, black, black, black, black, black, black, black, black,
    black, black, black, black, black, black, black, black, black, black
};


#define SCROLL rainbow

WS2812<numleds> leds(spi_transmit_dma);


void consumer(void *argv){

    spi_dma_init();
    while(1){
        Lock<Mutex> lock(mutex); //using RAII mutex

        full.wait(lock);
        leds.show();
        ledOff();
    }
}

int main(){  

    Thread *consumer_thread;
    consumer_thread=Thread::create(consumer,256,1);


    while(1){
        for(int i = SCROLL.size(); i > 0 ; i--){
            {
                Lock<Mutex> lock(mutex); //using RAII mutex

                for(int j = 0; j < numleds; j++){
                    leds.setPixel(j, SCROLL[(j+i)%SCROLL.size()]);
                }
                full.signal();
            }
            Thread::sleep(50);
        }
    }
     consumer_thread->join();
}

