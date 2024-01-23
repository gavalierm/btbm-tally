static bool cmpColor(int arr1[3], int arr2[3]){
    if (arr1[0] == arr2[0] && arr1[1] == arr2[1] && arr1[2] == arr2[2]) {
        return true;
    }
    return false;
}

static void do_signal_color(int red, int green, int blue, int luma){
    if (luma < 0) {
        luma = 0;
    } else if (luma > 255) {
        luma = 255;
    }
    
    if(signaling == 0){
        //last color is set only for non-signaling signals
        last_color[0] = red;
        last_color[1] = green;
        last_color[2] = blue;
        last_color[3] = luma;
    }

    red = (red * luma) / 255;
    green = (green * luma) / 255;
    blue = (blue * luma) / 255;

    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_set_pixel(led_strip, 0, green, red, blue); //GBR
    /* Refresh the strip to send data */
    led_strip_refresh(led_strip);
}

static void do_signal_last(){
    do_signal_color(last_color[0],last_color[1],last_color[2],last_color[3]);
}

static void do_signal(int color[3])
{
    if(cmpColor(color, black)){
        led_strip_clear(led_strip);
        return;
    }
    //luminance is global
    return do_signal_color(color[0], color[1], color[2], luminance);
}

static void do_signal_blink_panic(int color[3])
{
    int count = 5;
    int blink_len = 50;
    //
    for (int i = 0; i < count; ++i)
    {
        do_signal(color);
        vTaskDelay(blink_len / portTICK_PERIOD_MS);
        do_signal(black);
        vTaskDelay((CONFIG_INTERNAL_LED_PERIOD - (blink_len * count)) / count / portTICK_PERIOD_MS);
    }
}

static void do_signal_blink(int color[3])
{
    int count = 10;
    int blink_len = 50;
    //
    do_signal(color);
    vTaskDelay(blink_len / portTICK_PERIOD_MS);
    do_signal(black);
    vTaskDelay((CONFIG_INTERNAL_LED_PERIOD - (blink_len * count)) / count / portTICK_PERIOD_MS);
}

static void do_signal_no_period(int color[3])
{
    int blink_len = 50;
    //
    do_signal(color);
    vTaskDelay(blink_len / portTICK_PERIOD_MS);
    do_signal(black);
}


static bool check_signaling(){
    switch (signaling) {
        case 0:
            // return to last state
            do_signal_last();
            return false;
            break;
        case 1:
            do_signal(yellow);
            return true;
            break;
        case 2:
            //this is one time signal for operator
            do_signal_blink_panic(yellow);
            do_signal_blink_panic(yellow);
            do_signal_blink_panic(yellow);
            // return to last state
            do_signal_last();
            //reset signaling
            signaling = 0;
            return true;
            break;
        case 3:
            do_signal(blue);
            return true;
            break;
    }
    return false;
}

static void store_signal(int color[3]){
    last_color[0] = color[0];
    last_color[1] = color[1];
    last_color[2] = color[2];
    last_color[3] = luminance;
}
static void store_signal_color(int red, int green, int blue, int luma){
    last_color[0] = red;
    last_color[1] = green;
    last_color[2] = blue;
    last_color[3] = luma;
}


static void configure_led(void)
{
    ESP_LOGI(LED_TAG, "Example configured to do_signal addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_SHIELD_GPIO,
        .max_leds = LED_SHIELD_LENGHT, // at least one LED on board
    };
#if CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_INTERNAL_LED_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}