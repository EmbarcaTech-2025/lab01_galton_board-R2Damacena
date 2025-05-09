
# Projetos de Sistemas Embarcados - EmbarcaTech 2025

Autor: **Arthur Damacena Silva**

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Brasília, maio de 2025

---

# 🌐 Galton Board Digital

Este projeto implementa uma **versão digital** de uma Galton Board, demonstrando como múltiplas decisões aleatórias levam a uma **distribuição normal**. A simulação permite visualizar o comportamento estatístico de partículas caindo através de pinos, criando um histograma em tempo real.

---

## 🎯 Objetivo do Projeto

Criar uma simulação interativa de uma Galton Board que:

- Demonstre visualmente o princípio da **distribuição normal**.
- Permita a **interação do usuário** para adicionar bolas e alterar parâmetros.
- Ofereça **controle sobre o desvio** para experimentação com distribuições assimétricas.
- Visualize os resultados em tempo real através de um **histograma dinâmico**.

---

## 🔧 Componentes Usados

| Componente              | Descrição                        |
| ----------------------- | -------------------------------- |
| Raspberry Pi Pico       | Microcontrolador RP2040          |
| Display OLED SSD1306    | Display 128x64 pixels com I2C    |
| 2 Botões                | Interação com o usuário          |
| Joystick analógico      | Controle do desvio na simulação  |

---

## 💾 Como Compilar e Executar o Código

1. **Clone o repositório** e abra no **VS Code** com a extensão **CMake** instalada.
2. Configure o SDK do Raspberry Pi Pico:
   - Conecte o Raspberry Pi Pico em modo **bootloader** (segure o botão `BOOTSEL` enquanto conecta o USB).
   - Copie o arquivo gerado `lab-01-galton-board.uf2` para a unidade de disco do Pico.
3. O dispositivo **reiniciará automaticamente** e executará a simulação.

---

## ⚡ Pinagem dos Dispositivos Utilizados

| Componente               | Pinos no Raspberry Pi Pico           |
| ------------------------ | ------------------------------------ |
| Display OLED SSD1306     | SDA: GPIO14, SCL: GPIO15 (I2C1)      |
| Botão A (Visualizar)     | GPIO5 (com pull-up interno)          |
| Botão B (Adicionar)      | GPIO6 (com pull-up interno)          |
| Joystick X (Desvio)      | GPIO27 (ADC1)                        |

---

## 🖼️ Imagens do Projeto em Funcionamento

### 🎲 Modo Simulação
Exibe as bolas caindo através dos pinos em tempo real, com o indicador de desvio no topo.

### 📊 Modo Histograma
Mostra a distribuição resultante das bolas que caíram, formando uma curva normal.

---

## 📈 Resultados Esperados ou Obtidos

- **Distribuição Normal**: Com o desvio (bias) em zero, as bolas devem formar uma distribuição aproximadamente normal (curva de sino).
- **Interatividade**: O usuário pode:
  - Pressionar o **botão B** para adicionar uma nova bola.
  - Manter pressionado o botão B para adicionar múltiplas bolas em sequência.
  - Pressionar o **botão A** para alternar entre visualização da simulação e histograma.
  - Usar o **joystick** para alterar o desvio (bias), tornando a distribuição assimétrica.
- **Aprendizado Estatístico**: Demonstra visualmente como eventos aleatórios se comportam em conjunto, criando padrões previsíveis.

---

## 🧪 Experimentos Possíveis

- Compare diferentes níveis de desvio e observe como isso afeta a distribuição final.
- Teste o comportamento com grande número de amostras para verificar a convergência para a distribuição normal.
- Observe como as posições extremas são menos frequentes que as posições centrais.


---

## 📜 Licença
MIT License - MIT GPL-3.0.

