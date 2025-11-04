#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(meu_modulo, LOG_LEVEL_DBG);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led2)



// Verifica se os LEDs Vermelho e verde estão definidos no Device Tree
#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(LED1_NODE, okay)
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
#else
#error "Unsupported board: led1 devicetree alias is not defined"
#endif



#define PRIO_LEDTHREADS 5
#define PRIO_MODO_NOT 4

bool modo_noturno = true;
int Alternar = 0; //0 Verde , 1 Vermelho

k_tid_t Thread_Rodando;

K_MUTEX_DEFINE(ModeChange);





void Thread_Vermelho(void* a1 , void* a2 , void* a3)
{
    k_mutex_lock(&ModeChange , K_FOREVER);
    if(!gpio_pin_get_dt(&led2)) gpio_pin_toggle_dt(&led2);
    k_mutex_unlock(&ModeChange);

    k_msleep(2000);
    gpio_pin_toggle_dt(&led2);

    k_mutex_lock(&ModeChange , K_FOREVER);
    Thread_Rodando = 0;
    Alternar = 0;
    k_mutex_unlock(&ModeChange);
}

void Thread_Verde(void* a1 , void* a2 , void* a3)
{
    k_mutex_lock(&ModeChange , K_FOREVER);
    if(!gpio_pin_get_dt(&led0)) gpio_pin_toggle_dt(&led0);
    k_mutex_unlock(&ModeChange);

    k_msleep(4000);
    gpio_pin_toggle_dt(&led0);

    k_mutex_lock(&ModeChange , K_FOREVER);
    Thread_Rodando = 0;
    Alternar = 1;
    k_mutex_unlock(&ModeChange);
}

void Thread_Modo_Noturno(void* a1 , void* a2 , void* a3)
{
    k_mutex_lock(&ModeChange , K_FOREVER);
    if(!gpio_pin_get_dt(&led2)) gpio_pin_toggle_dt(&led2);
    if(gpio_pin_get_dt(&led0)) gpio_pin_toggle_dt(&led0);
    k_mutex_unlock(&ModeChange);

    k_msleep(1000);
    gpio_pin_toggle_dt(&led2);
    k_msleep(1000);

    k_mutex_lock(&ModeChange , K_FOREVER);
    Thread_Rodando = 0;
    k_mutex_unlock(&ModeChange);
}

K_THREAD_STACK_DEFINE(Stack_Vermelho , 512);
K_THREAD_STACK_DEFINE(Stack_Verde , 512);
K_THREAD_STACK_DEFINE(Stack_Modo_noturno , 512);

struct k_thread t_vermelho;
struct k_thread t_verde;
struct k_thread t_modo_noturno;


#define BUTTON_NODE DT_NODELABEL(user_button_0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

void botao_de_pedestre(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if(modo_noturno) return;
    
    if(Alternar == 0)
    {
        k_thread_abort(Thread_Rodando);
        Thread_Rodando =  k_thread_create(&t_verde, Stack_Verde,
                    K_THREAD_STACK_SIZEOF(Stack_Verde),
                    Thread_Verde,
                    NULL, NULL, NULL,
                    PRIO_LEDTHREADS, 0, K_NO_WAIT);
        
    }
    else
    {
        k_thread_abort(Thread_Rodando);

        if(!gpio_pin_get_dt(&led2)) gpio_pin_toggle_dt(&led2);
        k_msleep(1000);
        gpio_pin_toggle_dt(&led2);

        Thread_Rodando =  k_thread_create(&t_verde, Stack_Verde,
                    K_THREAD_STACK_SIZEOF(Stack_Verde),
                    Thread_Verde,
                    NULL, NULL, NULL,
                    PRIO_LEDTHREADS, 0, K_NO_WAIT);
    }
}

void main(void)
{
    int ret;

    // Verifica se o device está pronto
    if (!gpio_is_ready_dt(&led0)) {
        LOG_ERR("Error: LED device %s is not ready\n", led0.port->name);
        return;
    }
    // Verifica se o device está pronto
    if (!gpio_is_ready_dt(&led2)) {
        LOG_ERR("Error: LED device %s is not ready\n", led2.port->name);
        return;
    }
    

    // Configura o pino como saída
    ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Error %d: failed to configure LED pin\n", ret);
        return;
    }
    ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Error %d: failed to configure LED pin\n", ret);
        return;
    }
    
    Thread_Rodando = 0;

    while(1)
    {
        if(modo_noturno)
        {
            if(Thread_Rodando == 0)
            {
                Thread_Rodando =  k_thread_create(&t_modo_noturno, Stack_Modo_noturno,
                    K_THREAD_STACK_SIZEOF(Stack_Modo_noturno),
                    Thread_Modo_Noturno,
                    NULL, NULL, NULL,
                    PRIO_MODO_NOT, 0, K_NO_WAIT);
            }
            else if(k_thread_priority_get(Thread_Rodando) != 4)
            {
                k_thread_abort(Thread_Rodando);
                Thread_Rodando =  k_thread_create(&t_modo_noturno, Stack_Modo_noturno,
                    K_THREAD_STACK_SIZEOF(Stack_Modo_noturno),
                    Thread_Modo_Noturno,
                    NULL, NULL, NULL,
                    PRIO_MODO_NOT, 0, K_NO_WAIT);
            }
        }
        else if(Thread_Rodando == 0)
        {
            if(Alternar == 0)
            {
                Thread_Rodando =  k_thread_create(&t_verde, Stack_Verde,
                    K_THREAD_STACK_SIZEOF(Stack_Verde),
                    Thread_Verde,
                    NULL, NULL, NULL,
                    PRIO_LEDTHREADS, 0, K_NO_WAIT);
            }
            else
            {
                Thread_Rodando =  k_thread_create(&t_vermelho, Stack_Vermelho,
                    K_THREAD_STACK_SIZEOF(Stack_Vermelho),
                    Thread_Vermelho,
                    NULL, NULL, NULL,
                    PRIO_LEDTHREADS, 0, K_NO_WAIT);
            }
        }

        k_msleep(1);
    }
}
