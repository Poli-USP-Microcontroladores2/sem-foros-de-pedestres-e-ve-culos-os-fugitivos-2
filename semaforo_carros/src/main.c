#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <string.h>

#define LED_RED_NODE   DT_ALIAS(led2)
#define LED_GREEN_NODE DT_ALIAS(led0)
#define LED_BLUE_NODE  DT_ALIAS(led1)  /* “yellow” lógico usa verde+vermelho */

#if !DT_NODE_HAS_STATUS(LED_RED_NODE, okay) || \
    !DT_NODE_HAS_STATUS(LED_GREEN_NODE, okay) || \
    !DT_NODE_HAS_STATUS(LED_BLUE_NODE, okay)
#error "Board must have led0/led1/led2 aliases"
#endif

static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);

K_MUTEX_DEFINE(led_mutex);
K_SEM_DEFINE(sem_noturno, 0, 1);  // semáforo para modo noturno

#define T_RED    4000
#define T_GREEN  3000
#define T_YELLOW 1000

volatile bool modo_noturno = false;  // flag global

void blink_thread_with_mutex(void *p1, void *p2, void *p3)
{
    const char *name = (const char *)p2;
    int tempo = (int)(intptr_t)p3;

    while (1) {
        if (modo_noturno) {
            k_msleep(100);
            continue; // pausa se modo noturno ativo
        }

        k_mutex_lock(&led_mutex, K_FOREVER);

        if (modo_noturno) {
            // caso tenha virado noturno enquanto esperava o mutex
            k_mutex_unlock(&led_mutex);
            continue;
        }

        if (strcmp(name, "RED") == 0) {
            gpio_pin_set_dt(&led_red, 1);
        } else if (strcmp(name, "GREEN") == 0) {
            gpio_pin_set_dt(&led_green, 1);
        } else if (strcmp(name, "YELLOW") == 0) {
            gpio_pin_set_dt(&led_red, 1);
            gpio_pin_set_dt(&led_green, 1);
        }

        printk("%s: ON for %d ms\n", name, tempo);
        k_msleep(tempo);

        gpio_pin_set_dt(&led_red, 0);
        gpio_pin_set_dt(&led_green, 0);
        gpio_pin_set_dt(&led_blue, 0);

        k_mutex_unlock(&led_mutex);
        k_msleep(200);
    }
}

/* --- Thread que dispara o semáforo --- */
void temporizador_noturno_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        k_msleep(25000);  // aguarda 25s
        k_sem_give(&sem_noturno);  // sinaliza para o modo noturno
    }
}

/* --- Thread do modo noturno --- */
void modo_noturno_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        k_sem_take(&sem_noturno, K_FOREVER); // espera o semáforo
        modo_noturno = true;

        printk(">>> Entrando no modo NOTURNO <<<\n");
        k_mutex_lock(&led_mutex, K_FOREVER);

        // pisca amarelo 6 vezes (exclusividade garantida)
        for (int i = 0; i < 6; i++) {
            gpio_pin_set_dt(&led_red, 1);
            gpio_pin_set_dt(&led_green, 1);
            printk("[Modo Noturno] Amarelo piscando... (%d/6)\n", i + 1);
            k_msleep(500);
            gpio_pin_set_dt(&led_red, 0);
            gpio_pin_set_dt(&led_green, 0);
            k_msleep(500);
        }

        gpio_pin_set_dt(&led_red, 0);
        gpio_pin_set_dt(&led_green, 0);
        gpio_pin_set_dt(&led_blue, 0);

        k_mutex_unlock(&led_mutex);
        modo_noturno = false;
        printk(">>> Saindo do modo NOTURNO <<<\n");
    }
}

/* stacks */
K_THREAD_STACK_DEFINE(stack_r, 768);
K_THREAD_STACK_DEFINE(stack_g, 768);
K_THREAD_STACK_DEFINE(stack_y, 768);
K_THREAD_STACK_DEFINE(stack_n, 768);
K_THREAD_STACK_DEFINE(stack_t, 768);  // thread temporizador

static struct k_thread t_r;
static struct k_thread t_g;
static struct k_thread t_y;
static struct k_thread t_n;
static struct k_thread t_t; // temporizador

void main(void)
{
    if (!gpio_is_ready_dt(&led_red) || !gpio_is_ready_dt(&led_green) || !gpio_is_ready_dt(&led_blue)) {
        printk("MAIN: some LED GPIO not ready\n");
        return;
    }

    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    printk("MAIN: starting threads\n");

    k_thread_create(&t_r, stack_r, K_THREAD_STACK_SIZEOF(stack_r),
                    blink_thread_with_mutex, NULL, (void*)"RED", (void*)(intptr_t)T_RED,
                    5, 0, K_NO_WAIT);

    k_thread_create(&t_g, stack_g, K_THREAD_STACK_SIZEOF(stack_g),
                    blink_thread_with_mutex, NULL, (void*)"GREEN", (void*)(intptr_t)T_GREEN,
                    5, 0, K_NO_WAIT);

    k_thread_create(&t_y, stack_y, K_THREAD_STACK_SIZEOF(stack_y),
                    blink_thread_with_mutex, NULL, (void*)"YELLOW", (void*)(intptr_t)T_YELLOW,
                    5, 0, K_NO_WAIT);

    k_thread_create(&t_n, stack_n, K_THREAD_STACK_SIZEOF(stack_n),
                    modo_noturno_thread, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);

    k_thread_create(&t_t, stack_t, K_THREAD_STACK_SIZEOF(stack_t),
                    temporizador_noturno_thread, NULL, NULL, NULL,
                    6, 0, K_NO_WAIT);

    while (1) {
        k_msleep(1000);
    }
}
