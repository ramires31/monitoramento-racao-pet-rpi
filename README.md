# 🐾 Monitoramento Inteligente de Alimentação de Pets (C + Raspberry Pi)

Sistema embarcado que utiliza Raspberry Pi, C e visão computacional (OpenCV) para monitorar o nível de ração de pets via webcam. Analisa padrões de consumo e envia alertas via Telegram sobre níveis baixos ou comportamento alimentar anormal, auxiliando na detecção precoce de problemas de saúde.

## 📝 Motivação

Alterações nos hábitos alimentares são indicadores precoces de problemas de saúde em animais de estimação. Este projeto visa criar uma solução automatizada e de baixo custo para monitorar o consumo de ração, superando a imprecisão do acompanhamento manual e fornecendo dados úteis para o tutor.

## ✨ Funcionalidades

* **Monitoramento de Nível:** Análise de imagem da tigela via OpenCV para determinar o nível atual de ração.
* **Cálculo de Consumo:** Estimativa da porcentagem e quantidade de ração consumida.
* **Análise Comportamental:** Registro da frequência e horários das refeições para estabelecer um padrão e detectar desvios (refeições perdidas, alterações no consumo).
* **Alertas via Telegram:** Notificações em tempo real para o tutor em caso de:
    * Nível de ração abaixo de um limiar crítico.
    * Detecção de comportamento alimentar fora do padrão (indicativo de possível problema de saúde).
* **Log de Consumo:** (Opcional) Registro histórico dos eventos de alimentação para análise posterior.

## 🛠️ Tecnologias Utilizadas

**Hardware:**
* Raspberry Pi 4 Model B (recomendado 4GB RAM)
* Webcam USB
* Cartão microSD (mínimo 16GB)
* Fonte de Alimentação para RPi
* (Opcional) Suporte para câmera

**Software:**
* **Sistema Operacional:** Raspberry Pi OS (baseado em Debian/Linux)
* **Linguagem:** C
* **Compilador:** GCC
* **Bibliotecas Principais:**
    * OpenCV (`libopencv-dev`): Para processamento de imagem e visão computacional.
    * `libcurl` (`libcurl4-openssl-dev`): Para realizar requisições HTTPS à API do Telegram.
    * `pthreads` (`-pthread`): Para gerenciamento de threads e concorrência.
    * `stdio.h`, `stdlib.h`, `string.h`, `unistd.h`, etc.: Bibliotecas padrão do C.

## 🚀 Como Compilar e Executar

1.  **Clone o Repositório:**
    ```bash
    git clone [URL_DO_SEU_REPOSITORIO]
    cd [NOME_DO_SEU_REPOSITORIO]
    ```
2.  **Instale as Dependências:**
    ```bash
    sudo apt update
    sudo apt install build-essential libopencv-dev libcurl4-openssl-dev git
    ```
3.  **Configure:** (Se necessário, edite arquivos de configuração para inserir o Token do Bot do Telegram e o Chat ID).
4.  **Compile:**
    * Se houver um `Makefile`:
        ```bash
        make
        ```
    * Se não houver, use o comando `gcc` apropriado (verifique as flags de linkagem `-lopencv_core -lcurl -pthread`, etc.). Exemplo:
        ```bash
        gcc seu_main.c seus_modulos.c -o monitor_racao -lopencv_core -lopencv_imgproc -lcurl -pthread -lm 
        ```
5.  **Execute:**
    ```bash
    ./monitor_racao [argumentos_opcionais]
    ```

## 🎓 Contexto

Este projeto foi desenvolvido como parte da disciplina de Sistemas Operacionais Embarcados da Universidade de Brasília (UnB).
