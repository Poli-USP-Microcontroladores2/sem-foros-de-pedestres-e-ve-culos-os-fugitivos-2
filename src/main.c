#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(meu_modulo, LOG_LEVEL_DBG);

#define LED0_NODE DT_ALIAS(led0)   // Verde
#define LED2_NODE DT_ALIAS(led2)   // Vermelho

#define BUTTON_NODE0 DT_NODELABEL(user_button_0)
static const struct gpio_dt_spec button_ped = GPIO_DT_SPEC_GET(BUTTON_NODE0, gpios);
static struct gpio_callback button_cb_data;

#define BUTTON_NODE1 DT_NODELABEL(user_button_1)
static const struct gpio_dt_spec button_Sinc = GPIO_DT_SPEC_GET(BUTTON_NODE1, gpios);
static struct gpio_callback button_cb_data2;

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#error "Unsupported board: led0 not found"
#endif

#if DT_NODE_HAS_STATUS(LED2_NODE, okay)
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
#else
#error "Unsupported board: led2 not found"
#endif

#define PTA5_NODE DT_NODELABEL(gpioa)
#define PTA5_PIN 5
const struct device *gpioa = DEVICE_DT_GET(PTA5_NODE);

#define PRIO_LEDTHREADS 5

int Alternar = 0;  // 0 = Verde, 1 = Amarelo, 2 = Vermelho
bool Sinc = false;
k_tid_t Thread_Rodando;
K_MUTEX_DEFINE(ModeChange);

K_THREAD_STACK_DEFINE(Stack_Verde , 512);
K_THREAD_STACK_DEFINE(Stack_Amarelo , 512);
K_THREAD_STACK_DEFINE(Stack_Vermelho , 512);

struct k_thread t_verde;
struct k_thread t_amarelo;
struct k_thread t_vermelho;


// ====================== THREADS ======================

void Thread_Verde(void* a1 , void* a2 , void* a3)
{
    k_mutex_lock(&ModeChange , K_FOREVER);
    if(!gpio_pin_get_dt(&led0)) gpio_pin_toggle_dt(&led0); // Liga verde
    k_mutex_unlock(&ModeChange);

    k_msleep(3000);

    k_mutex_lock(&ModeChange , K_FOREVER);
    gpio_pin_toggle_dt(&led0); // Desliga verde

    gpio_pin_set(gpioa, PTA5_PIN, 1);
    k_usleep(100);
    gpio_pin_set(gpioa, PTA5_PIN, 0);

    Alternar = 1;
    Thread_Rodando = 0;
    k_mutex_unlock(&ModeChange);
}

void Thread_Amarelo(void* a1 , void* a2 , void* a3)
{
    k_mutex_lock(&ModeChange , K_FOREVER);

    // Amarelo = verde ligado + vermelho ligado
    if(!gpio_pin_get_dt(&led0)) gpio_pin_toggle_dt(&led0); 
    if(!gpio_pin_get_dt(&led2)) gpio_pin_toggle_dt(&led2);

    k_mutex_unlock(&ModeChange);

    k_msleep(1000);

    k_mutex_lock(&ModeChange , K_FOREVER);

    // Desliga ambos
    gpio_pin_toggle_dt(&led0);
    gpio_pin_toggle_dt(&led2);

    gpio_pin_set(gpioa, PTA5_PIN, 1);
    k_usleep(100);
    gpio_pin_set(gpioa, PTA5_PIN, 0);

    Alternar = 2;
    Thread_Rodando = 0;
    k_mutex_unlock(&ModeChange);
}

void Thread_Vermelho(void* a1 , void* a2 , void* a3)
{
    k_mutex_lock(&ModeChange , K_FOREVER);
    if(!gpio_pin_get_dt(&led2)) gpio_pin_toggle_dt(&led2); // Liga vermelho
    k_mutex_unlock(&ModeChange);

    k_msleep(4000);

    k_mutex_lock(&ModeChange , K_FOREVER);
    gpio_pin_toggle_dt(&led2); // Desliga vermelho

    gpio_pin_set(gpioa, PTA5_PIN, 1);
    k_usleep(100);
    gpio_pin_set(gpioa, PTA5_PIN, 0);

    Alternar = 0;
    Thread_Rodando = 0;
    k_mutex_unlock(&ModeChange);
}


// ====================== MAIN ======================

void main(void)
{
    if (!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led2)) {
        LOG_ERR("LEDs not ready");
        return;
    }

    gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);

    if (!device_is_ready(gpioa)) return;
    gpio_pin_configure(gpioa, PTA5_PIN, GPIO_OUTPUT_ACTIVE);

    LOG_INF("Semáforo iniciado");

    while(1)
    {
        if(!Sinc && Thread_Rodando == 0)
        {
            if(Alternar == 0)
                Thread_Rodando = k_thread_create(&t_verde, Stack_Verde,
                    K_THREAD_STACK_SIZEOF(Stack_Verde), Thread_Verde,
                    NULL, NULL, NULL, PRIO_LEDTHREADS, 0, K_NO_WAIT);

            else if(Alternar == 1)
                Thread_Rodando = k_thread_create(&t_amarelo, Stack_Amarelo,
                    K_THREAD_STACK_SIZEOF(Stack_Amarelo), Thread_Amarelo,
                    NULL, NULL, NULL, PRIO_LEDTHREADS, 0, K_NO_WAIT);

            else if(Alternar == 2)
                Thread_Rodando = k_thread_create(&t_vermelho, Stack_Vermelho,
                    K_THREAD_STACK_SIZEOF(Stack_Vermelho), Thread_Vermelho,
                    NULL, NULL, NULL, PRIO_LEDTHREADS, 0, K_NO_WAIT);
        }
    }
}
