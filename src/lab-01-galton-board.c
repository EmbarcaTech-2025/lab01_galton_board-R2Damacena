#include <stdio.h>
#include <stdlib.h> 
#include <math.h>   
#include <string.h> 

#include "pico/stdlib.h"
#include "hardware/adc.h"

// Biblioteca para display SSD1306
#include "include/ssd1306.h"

// Antes das suas definições atuais, adicione:
#define CLAMP(value, min, max) (MIN(MAX((value), (min)), (max)))

// Display
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

// Hardware
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define I2C_PORT i2c1
#define JOYSTICK_X_GPIO 27
#define JOYSTICK_X_ADC_CHANNEL 1

// Joystick
#define RAW_X_MIN 12
#define RAW_X_MAX 4076
#define JOYSTICK_DEADZONE 200

// Simulação
#define MAX_BALLS 50
#define NUM_PIN_ROWS 12
#define PINS_PER_ROW(row) (row + 1)
#define BALL_RADIUS 1

// Layout visual
#define PIN_ROW_START_Y 8
#define PIN_ROW_SPACING 4
#define PIN_HORIZONTAL_SPACING 9
#define BOTTOM_Y (PIN_ROW_START_Y + (NUM_PIN_ROWS * PIN_ROW_SPACING) + 5)

// Histograma
#define NUM_BINS 13
#define BIN_WIDTH ((float)SSD1306_WIDTH / NUM_BINS)
#define MAX_HISTO_HEIGHT (SSD1306_HEIGHT - 15)

// Timing
#define LOOP_DELAY_MS 30
#define DEBOUNCE_DELAY_MS 150

// Estruturas e Enums
typedef struct
{
    float x;
    float y;
    bool active;
} Ball;

typedef enum
{
    VIEW_SIMULATION,
    VIEW_HISTOGRAM
} ViewState;

// Variáveis ​​Globais
uint8_t *display_buffer; // Buffer para o display
struct render_area area; // Área de renderização
Ball balls[MAX_BALLS];
uint16_t bins[NUM_BINS]; // Contagem para cada canaleta
uint32_t total_balls_dropped = 0;
ViewState current_view = VIEW_SIMULATION;

// Estado de debounce do botão
volatile uint32_t last_press_time_a = 0;
volatile uint32_t last_press_time_b = 0;
volatile bool button_a_pressed_flag = false;
volatile bool button_b_pressed_flag = false;
volatile bool button_a_held = false;

// Protótipos de Função
void init_hardware();
void init_simulation();
void handle_input();
void add_new_ball();
void update_simulation(float bias);
void process_pin_collisions(Ball *ball, float bias);
void deflect_ball(Ball *ball, float bias, float pin_x);
void process_ball_at_bottom(Ball *ball);
void keep_ball_in_bounds(Ball *ball);
float get_joystick_bias();
void draw_simulation(float bias);
void draw_histogram();
void draw_pins();
void gpio_callback(uint gpio, uint32_t events);
float map_range(float value, float in_min, float in_max, float out_min, float out_max);
void clear_display_buffer();
void draw_square(uint8_t *buffer, int x, int y, int width, int height);
void draw_pixel(uint8_t *buffer, int x, int y);
void update_display();
uint16_t find_max_bin_count();
int calculate_bar_height(uint16_t bin_count, uint16_t max_count);
int calculate_bar_width(int bin_index);
void draw_bin_count(int bin_index, int x_start, int bar_width);

// Função principal
int main()
{
    init_hardware();
    init_simulation();

    while (true)
    {
        handle_input(); // Processa as flags setadas pelas interrupções

        float bias = get_joystick_bias(); // Para causar desvio na simulação (Desbalanceamento Experimental)

        // Atualiza a simulação ou o histograma
        if (current_view == VIEW_SIMULATION)
        {
            update_simulation(bias);
            draw_simulation(bias);
        }
        else
        {
            draw_histogram();
        }

        // Mostra o buffer do display
        update_display();

        sleep_ms(LOOP_DELAY_MS);
    }
    return 0;
}

// Função auxiliar para limpar o buffer do display
void clear_display_buffer() {
    for (int i = 0; i < SSD1306_WIDTH * SSD1306_HEIGHT / 8; i++) {
        display_buffer[i] = 0;
    }
}

// Função auxiliar para desenhar um pixel usando a API disponível
void draw_pixel(uint8_t *buffer, int x, int y) {
    if (x >= 0 && x < SSD1306_WIDTH && y >= 0 && y < SSD1306_HEIGHT) {
        ssd1306_set_pixel(buffer, x, y, true);
    }
}

// Função auxiliar para desenhar quadrados usando a API disponível
void draw_square(uint8_t *buffer, int x, int y, int width, int height) {
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            draw_pixel(buffer, x + i, y + j);
        }
    }
}

// Atualiza o display com o buffer atual
void update_display() {
    render_on_display(display_buffer, &area);
}

// Inicialização de hardware
void init_hardware()
{
    stdio_init_all();

    // Inicialização I2C
    i2c_init(I2C_PORT, 400 * 1000); // 400kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Inicialização do display SSD1306
    ssd1306_init();
    
    // Configurar a área de renderização para ocupar toda a tela
    area.start_column = 0;
    area.end_column = SSD1306_WIDTH - 1;
    area.start_page = 0;
    area.end_page = SSD1306_HEIGHT / 8 - 1;
    calculate_render_area_buffer_length(&area);
    
    // Alocar o buffer para o display
    display_buffer = malloc(area.buffer_length);
    clear_display_buffer();
    
    // Inicialização do ADC
    adc_init();
    adc_gpio_init(JOYSTICK_X_GPIO);

    // Inicialização de botões com interrupções
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    // Configurar para detectar apenas a queda de borda inicialmente
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Seed para gerador de números aleatórios
    srand(time_us_32());

}

// Inicialização da Simulação
void init_simulation()
{
    for (int i = 0; i < MAX_BALLS; ++i)
    {
        balls[i].active = false;
    }
    for (int i = 0; i < NUM_BINS; ++i)
    {
        bins[i] = 0;
    }
    total_balls_dropped = 0;
    current_view = VIEW_SIMULATION;
}

// Tratamento de interrupção GPIO
void gpio_callback(uint gpio, uint32_t events)
{
    uint32_t now = time_us_32();
    
    // Botão B (Adicionar Bola) - quando pressionado
    if (gpio == BUTTON_B_PIN) 
    {
        if (events == GPIO_IRQ_EDGE_FALL) // Botão pressionado
        {
            // Debounce simples baseado em horário
            if ((int32_t)(now - last_press_time_a) > DEBOUNCE_DELAY_MS * 1000 || last_press_time_a == 0)
            {
                button_a_pressed_flag = true;
                button_a_held = true;
                last_press_time_a = now;
                
                // Ativa a interrupção para subida de borda (quando o botão for solto)
                gpio_set_irq_enabled(BUTTON_B_PIN, GPIO_IRQ_EDGE_RISE, true);
            }
        }
        else if (events == GPIO_IRQ_EDGE_RISE) // Botão solto
        {
            button_a_held = false;
            // Desativa a interrupção de subida quando não é mais necessária
            gpio_set_irq_enabled(BUTTON_B_PIN, GPIO_IRQ_EDGE_RISE, false);
        }
    }
    
    // Botão A (Alternar Visualização)
    else if (gpio == BUTTON_A_PIN && events == GPIO_IRQ_EDGE_FALL)
    {
        // Debounce simples baseado em horário
        if ((int32_t)(now - last_press_time_b) > DEBOUNCE_DELAY_MS * 1000 || last_press_time_b == 0)
        {
            button_b_pressed_flag = true;
            last_press_time_b = now;
        }
    }
}

// Tratamento do input (chamada do loop principal)
void handle_input()
{
    // Adiciona bola quando o botão é pressionado pela primeira vez
    if (button_a_pressed_flag)
    {
        add_new_ball();
        button_a_pressed_flag = false; // Reseta a flag imediatamente após o tratamento
    }
    
    // Modo spam: adiciona mais bolas enquanto o botão estiver pressionado
    if (button_a_held && current_view == VIEW_SIMULATION)
    {
        // Limita a taxa para não sobrecarregar
        static uint32_t last_spam_time = 0;
        uint32_t now = time_us_32();
        
        if ((now - last_spam_time) > 100000) // 100ms entre bolas (ajuste conforme necessário)
        {
            add_new_ball();
            last_spam_time = now;
        }
    }
    
    // Alterna visualização
    if (button_b_pressed_flag)
    {
        current_view = (current_view == VIEW_SIMULATION) ? VIEW_HISTOGRAM : VIEW_SIMULATION;
        button_b_pressed_flag = false; // Reseta a flag imediatamente após o tratamento
    }
}

// Adiciona uma nova bola na simulação
void add_new_ball()
{
    // Verifica se estamos na visualização correta
    if (current_view != VIEW_SIMULATION)
        return;
        
    // Percorre o array de bolas para encontrar uma vaga
    for (int i = 0; i < MAX_BALLS; ++i)
    {
        if (!balls[i].active)
        {
            balls[i].x = SSD1306_WIDTH / 2.0f;
            balls[i].y = 0.0f;
            balls[i].active = true;
            return;
        }
    }
    // Se chegar aqui, não há slots disponíveis para novas bolas
}

// Atualiza Estado da Simulação
void update_simulation(float bias)
{
    for (int i = 0; i < MAX_BALLS; ++i)
    {
        if (!balls[i].active) continue;
        
        // Move a bola para baixo
        balls[i].y += 1.0f;
        
        // Processa colisões com pinos
        process_pin_collisions(&balls[i], bias);
        
        // Verifica se alcançou o fundo
        if (balls[i].y >= BOTTOM_Y) {
            process_ball_at_bottom(&balls[i]);
            continue;
        }
        
        // Mantém a bola dentro dos limites laterais
        keep_ball_in_bounds(&balls[i]);
    }
}

void process_pin_collisions(Ball *ball, float bias)
{
    for (int row = 0; row < NUM_PIN_ROWS; ++row)
    {
        float row_y = PIN_ROW_START_Y + row * PIN_ROW_SPACING;
        
        // Verifica se a bola está no nível desta fileira de pinos
        if (ball->y >= row_y && ball->y < row_y + 1.0f)
        {
            float row_width = (PINS_PER_ROW(row) - 1) * PIN_HORIZONTAL_SPACING;
            float row_start_x = (SSD1306_WIDTH - row_width) / 2.0f;
            
            // Verifica colisão com cada pino na linha
            for (int pin_idx = 0; pin_idx < PINS_PER_ROW(row); ++pin_idx)
            {
                float pin_x = row_start_x + pin_idx * PIN_HORIZONTAL_SPACING;
                
                // Se está próximo ao pino, decide a direção
                if (fabs(ball->x - pin_x) < PIN_HORIZONTAL_SPACING / 2.0f)
                {
                    deflect_ball(ball, bias, pin_x);
                    // Empurra a bola para a próxima linha (evita colisões repetidas)
                    ball->y = row_y + 1.0f;
                    return;
                }
            }
        }
    }
}

void deflect_ball(Ball *ball, float bias, float pin_x)
{
    // O desvio afeta a chance da bola ir para direita
    int threshold = 50 + (int)(bias * 40.0f);
    threshold = CLAMP(threshold, 5, 95);
    
    // Determina direção baseado no threshold com viés
    if (rand() % 100 < threshold) {
        // Direita
        ball->x += PIN_HORIZONTAL_SPACING / 2.0f;
    } else {
        // Esquerda
        ball->x -= PIN_HORIZONTAL_SPACING / 2.0f;
    }
}

void process_ball_at_bottom(Ball *ball)
{
    // Garante que a bola está dentro dos limites horizontais
    ball->x = CLAMP(ball->x, 0, SSD1306_WIDTH - 1);
    
    // Calcula qual canaleta a bola caiu
    int bin_index = (int)floor(ball->x / BIN_WIDTH);
    bin_index = CLAMP(bin_index, 0, NUM_BINS - 1);
    
    // Incrementa contadores
    bins[bin_index]++;
    total_balls_dropped++;
    
    // Desativa a bola
    ball->active = false;
}

void keep_ball_in_bounds(Ball *ball)
{
    ball->x = CLAMP(ball->x, BALL_RADIUS, SSD1306_WIDTH - 1 - BALL_RADIUS);
}

// Lê o valor do joystick e calcula o desvio
float get_joystick_bias()
{
    adc_select_input(JOYSTICK_X_ADC_CHANNEL);
    uint16_t raw_x = adc_read();

    // Calcula o centro dinâmico do joystick
    int center_x = (RAW_X_MAX + RAW_X_MIN) / 2;
    int centered_x = raw_x - center_x;

    if (abs(centered_x) < JOYSTICK_DEADZONE)
    {
        return 0.0f; // Dentro da deadzone, sem desvio
    }

    // Mapeia o valor do joystick para um intervalo de -1.0 a 1.0
    float bias = 0.0f;
    if (centered_x > 0)
    { // Para direita
        bias = map_range(raw_x, center_x + JOYSTICK_DEADZONE, RAW_X_MAX, 0.0f, 1.0f);
    }
    else
    { // Para esquerda
        bias = map_range(raw_x, RAW_X_MIN, center_x - JOYSTICK_DEADZONE, -1.0f, 0.0f);
    }

    // Limita a polarização apenas no caso de o mapeamento sair um pouco dos limites
    if (bias < -1.0f)
        bias = -1.0f;
    if (bias > 1.0f)
        bias = 1.0f;

    return bias;
}

// Desenha a tela de simulação
void draw_simulation(float bias)
{
    clear_display_buffer();

    // Desenha pinos
    draw_pins();

    // Desenha canaletas
    for (int i = 0; i <= NUM_BINS; ++i)
    {
        int x = (int)(i * BIN_WIDTH);
        if (x >= SSD1306_WIDTH)
            x = SSD1306_WIDTH - 1; // Limita a boarda
        ssd1306_draw_line(display_buffer, x, BOTTOM_Y, x, SSD1306_HEIGHT - 1, true);
    }
    ssd1306_draw_line(display_buffer, 0, BOTTOM_Y, SSD1306_WIDTH - 1, BOTTOM_Y, true); // Linhas separando canaletas dos pinos

    // Desenha bolas ativas
    for (int i = 0; i < MAX_BALLS; ++i)
    {
        if (balls[i].active)
        {
            int ball_draw_x = (int)balls[i].x - BALL_RADIUS;
            int ball_draw_y = (int)balls[i].y - BALL_RADIUS;
            int ball_size = BALL_RADIUS * 2 + 1;

            // Garante que as coordenadas estejam dentro dos limites da tela
            if (ball_draw_x < 0)
                ball_draw_x = 0;
            if (ball_draw_y < 0)
                ball_draw_y = 0;
            if (ball_draw_x + ball_size > SSD1306_WIDTH)
                ball_draw_x = SSD1306_WIDTH - ball_size;
            if (ball_draw_y + ball_size > SSD1306_HEIGHT)
                ball_draw_y = SSD1306_HEIGHT - ball_size;

            draw_square(display_buffer, ball_draw_x, ball_draw_y, ball_size, ball_size);
        }
    }

    // Desenha a contagem total de bolas
    char count_str[20];
    sprintf(count_str, "N %lu", total_balls_dropped);
    ssd1306_draw_string(display_buffer, 1, 1, count_str);

    // Desenha o indicador de desvio
    int bias_bar_center_x = SSD1306_WIDTH - 15;
    int bias_bar_y = 5;
    int bias_bar_width = 10; // Largura total do intervalo do indicador
    // Linha representando o intervalo
    ssd1306_draw_line(display_buffer, bias_bar_center_x - bias_bar_width, bias_bar_y, bias_bar_center_x + bias_bar_width, bias_bar_y, true);
    // Posição do indicador com base no desvio
    int bias_indicator_x = bias_bar_center_x + (int)(bias * bias_bar_width);
    // Desenhe uma pequena linha vertical para o indicador
    ssd1306_draw_line(display_buffer, bias_indicator_x, bias_bar_y - 2, bias_indicator_x, bias_bar_y + 2, true);
}

// Desenha a tela do histograma
void draw_histogram()
{
    clear_display_buffer();

    // Encontra o valor máximo para escala do histograma
    uint16_t max_count = find_max_bin_count();

    // Desenha as barras do histograma
    for (int i = 0; i < NUM_BINS; ++i)
    {
        // Calcula dimensões da barra
        int bar_height = calculate_bar_height(bins[i], max_count);
        int x_start = (int)(i * BIN_WIDTH);
        int bar_width = calculate_bar_width(i);
        
        // Posição vertical da barra
        int y_start = SSD1306_HEIGHT - 1 - bar_height;
        y_start = MAX(y_start, 10); // Evita sobreposição com título
        
        // Desenha a barra se tiver dimensões válidas
        if (bar_height > 0 && bar_width > 0) {
            draw_square(display_buffer, x_start, y_start, bar_width, bar_height);
        }
        
        // Adiciona contagem numérica abaixo da barra
        draw_bin_count(i, x_start, bar_width);
    }

    // Mostra contagem total de bolas no título
    char title_str[25];
    sprintf(title_str, "N %lu", total_balls_dropped);
    ssd1306_draw_string(display_buffer, 1, 1, title_str);
}

// Encontra o valor máximo nas canaletas para escala do histograma
uint16_t find_max_bin_count() 
{
    uint16_t max_count = 0;
    for (int i = 0; i < NUM_BINS; ++i) {
        max_count = MAX(max_count, bins[i]);
    }
    return max_count;
}

// Calcula altura da barra considerando a escala
int calculate_bar_height(uint16_t bin_count, uint16_t max_count) 
{
    if (max_count == 0) return 0;
    
    int height = (int)(((float)bin_count / max_count) * MAX_HISTO_HEIGHT);
    return CLAMP(height, 0, MAX_HISTO_HEIGHT);
}

// Calcula largura da barra, considerando os limites da tela
int calculate_bar_width(int bin_index) 
{
    int x_start = (int)(bin_index * BIN_WIDTH);
    int width = (int)BIN_WIDTH - 1; // Espaço de 1px entre barras
    
    // Garante largura mínima
    width = MAX(width, 1);
    
    // Previne barras fora da tela
    if (x_start >= SSD1306_WIDTH) return 0;
    if (x_start + width >= SSD1306_WIDTH) {
        width = SSD1306_WIDTH - x_start - 1;
    }
    
    return width;
}

// Desenha o valor numérico abaixo da barra
void draw_bin_count(int bin_index, int x_start, int bar_width) 
{
    if (bins[bin_index] <= 0) return;
    
    char bin_count[6]; // Buffer para até 5 dígitos + terminador nulo
    sprintf(bin_count, "%d", bins[bin_index]);
    
    // Centraliza texto na barra
    int text_width = strlen(bin_count) * 6; // Estima 6 pixels por caractere
    int text_x = x_start + (bar_width / 2) - (text_width / 2);
    text_x = MAX(text_x, x_start);
    
    // Posiciona o texto próximo ao fundo do display
    int text_y = SSD1306_HEIGHT - 8;
    
    ssd1306_draw_string(display_buffer, text_x, text_y, bin_count);
}

// Desenha os pinos do Galton Board
void draw_pins()
{
    for (int row = 0; row < NUM_PIN_ROWS; ++row)
    {
        int num_pins_in_row = PINS_PER_ROW(row);
        float row_y = PIN_ROW_START_Y + row * PIN_ROW_SPACING;
        float row_width = (num_pins_in_row - 1) * PIN_HORIZONTAL_SPACING;
        float row_start_x = (SSD1306_WIDTH - row_width) / 2.0f;

        for (int pin_idx = 0; pin_idx < num_pins_in_row; ++pin_idx)
        {
            float pin_x = row_start_x + pin_idx * PIN_HORIZONTAL_SPACING;

            int px = (int)pin_x;
            int py = (int)row_y;

            // Checa se o pixel está dentro dos limites do display
            if (px >= 0 && px < SSD1306_WIDTH && py >= 0 && py < SSD1306_HEIGHT)
            {
                draw_pixel(display_buffer, px, py);
            }
        }
    }
}

// Função linear para mapear um valor de um intervalo para outro
float map_range(float value, float in_min, float in_max, float out_min, float out_max)
{
    // Evita divisão por zero
    if (fabs(in_max - in_min) < 1e-6)
    {
        return out_min + (out_max - out_min) / 2.0f; // Retorna o meio do intervalo de saída
    }
    // Calcula e retorna o valor mapeado
    return out_min + (out_max - out_min) * (value - in_min) / (in_max - in_min);
}
