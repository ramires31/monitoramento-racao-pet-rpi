#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <pigpio.h>
#include <curl/curl.h>

using namespace cv;
using namespace std;

// ===== CONFIGURAÇÕES =====
const int SERVO_PIN = 18;              // GPIO 18 (pino físico 12)
const int NIVEL_MINIMO = 20;           // Nível mínimo antes de reabastecer (%)
const int INTERVALO_VERIFICACAO = 3600; // Tempo entre verificações em segundos (3600 = 1 hora)
const int CAMERA_ID = 0;               // ID da webcam (0 = padrão)

// Posições do servo MG90S (valores em microsegundos para pigpio)
const int SERVO_FECHADO = 500;         // Porta fechada (500µs)
const int SERVO_ABERTO = 2400;         // Porta aberta (2400µs)
const int TEMPO_ALIMENTACAO = 1000;    // 1 segundo aberto
const int TEMPO_ABERTURA = 3000;       // Tempo total com porta aberta antes de fechar

// CONFIGURAÇÕES DO TELEGRAM
const char* TELEGRAM_TOKEN = "8484843988:AAFgNwxU1p9Fva683UwN9GBhV8ZY2h4dE90";
const char* TELEGRAM_CHAT_ID = "863810450";

// ===== VARIÁVEIS GLOBAIS =====
double densidadeCheio = 0;
double densidadeVazio = 0;

// ===== FUNÇÕES AUXILIARES =====

string obterTimestamp() {
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now_time));
    return string(buffer);
}

void salvarLog(const string& mensagem) {
    ofstream log("alimentador.log", ios::app);
    log << "[" << obterTimestamp() << "] " << mensagem << endl;
    log.close();
    cout << "[" << obterTimestamp() << "] " << mensagem << endl;
}

// ===== FUNÇÕES DO TELEGRAM =====

// Função para enviar mensagem de texto
bool enviarMensagemTelegram(const string& mensagem) {
    CURL *curl;
    CURLcode res;
    bool sucesso = false;
    
    curl = curl_easy_init();
    if (curl) {
        char *mensagem_encode = curl_easy_escape(curl, mensagem.c_str(), 0);
        
        char url[1024];
        snprintf(url, sizeof(url),
                 "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s",
                 TELEGRAM_TOKEN, TELEGRAM_CHAT_ID, mensagem_encode);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            sucesso = true;
            salvarLog("Mensagem enviada ao Telegram com sucesso!");
        } else {
            salvarLog("ERRO ao enviar mensagem: " + string(curl_easy_strerror(res)));
        }
        
        curl_free(mensagem_encode);
        curl_easy_cleanup(curl);
    }
    
    return sucesso;
}

// Função para enviar foto com legenda
bool enviarFotoTelegram(const string& caminhoFoto, const string& legenda) {
    CURL *curl;
    CURLcode res;
    bool sucesso = false;
    
    curl = curl_easy_init();
    if (curl) {
        char url[512];
        snprintf(url, sizeof(url),
                 "https://api.telegram.org/bot%s/sendPhoto",
                 TELEGRAM_TOKEN);
        
        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;
        
        // Adicionar chat_id
        curl_formadd(&formpost, &lastptr,
                     CURLFORM_COPYNAME, "chat_id",
                     CURLFORM_COPYCONTENTS, TELEGRAM_CHAT_ID,
                     CURLFORM_END);
        
        // Adicionar legenda
        curl_formadd(&formpost, &lastptr,
                     CURLFORM_COPYNAME, "caption",
                     CURLFORM_COPYCONTENTS, legenda.c_str(),
                     CURLFORM_END);
        
        // Adicionar foto
        curl_formadd(&formpost, &lastptr,
                     CURLFORM_COPYNAME, "photo",
                     CURLFORM_FILE, caminhoFoto.c_str(),
                     CURLFORM_END);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            sucesso = true;
            salvarLog("Foto enviada ao Telegram com sucesso!");
        } else {
            salvarLog("ERRO ao enviar foto: " + string(curl_easy_strerror(res)));
        }
        
        curl_easy_cleanup(curl);
        curl_formfree(formpost);
    }
    
    return sucesso;
}

// Função para enviar relatório completo (foto + informações)
void enviarRelatorioTelegram(double nivel, const string& status) {
    // Criar mensagem formatada
    string mensagem = "🐾 RELATÓRIO DO ALIMENTADOR 🐾\n\n";
    mensagem += "📊 Nível de ração: " + to_string((int)nivel) + "%\n";
    mensagem += "📌 Status: " + status + "\n";
    mensagem += "🕐 Horário: " + obterTimestamp() + "\n";
    
    if (nivel < NIVEL_MINIMO) {
        mensagem += "\n⚠️ ALERTA: Nível baixo!\n";
        mensagem += "🔄 Reabastecimento será iniciado.";
    } else {
        mensagem += "\n✅ Tudo OK!";
    }
    
    // Enviar foto com legenda
    enviarFotoTelegram("resultado.jpg", mensagem);
}

// ===== CONTROLE DO SERVO MG90S COM PIGPIO =====

bool inicializarServo() {
    // Inicializar pigpio
    if (gpioInitialise() < 0) {
        salvarLog("ERRO: Falha ao inicializar pigpio!");
        return false;
    }
    
    // Configurar pino como saída
    gpioSetMode(SERVO_PIN, PI_OUTPUT);
    
    // Definir posição inicial (fechado)
    gpioServo(SERVO_PIN, SERVO_FECHADO);
    salvarLog("Servo inicializado - Porta FECHADA");
    
    this_thread::sleep_for(chrono::milliseconds(500));
    return true;
}

void abrirPorta() {
    salvarLog(">>> ABRINDO porta do alimentador...");
    gpioServo(SERVO_PIN, SERVO_ABERTO);
    this_thread::sleep_for(chrono::milliseconds(TEMPO_ABERTURA));
}

void fecharPorta() {
    salvarLog(">>> FECHANDO porta do alimentador...");
    gpioServo(SERVO_PIN, SERVO_FECHADO);
    this_thread::sleep_for(chrono::milliseconds(1000));
}

// ===== CAPTURA DE FOTO =====

bool capturarFoto(const string& nomeArquivo) {
    VideoCapture cap(CAMERA_ID);
    
    if (!cap.isOpened()) {
        salvarLog("ERRO: Não foi possível abrir a câmera!");
        return false;
    }
    
    // Configurar resolução (opcional)
    cap.set(CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(CAP_PROP_FRAME_HEIGHT, 720);
    
    // Aguardar câmera estabilizar
    this_thread::sleep_for(chrono::milliseconds(1000));
    
    // Descartar primeiros frames
    Mat frame;
    for (int i = 0; i < 5; i++) {
        cap >> frame;
    }
    
    // Capturar frame final
    cap >> frame;
    
    if (frame.empty()) {
        salvarLog("ERRO: Frame vazio capturado!");
        cap.release();
        return false;
    }
    
    imwrite(nomeArquivo, frame);
    cap.release();
    salvarLog("Foto capturada: " + nomeArquivo);
    
    return true;
}

// ===== DETECÇÃO DE NÍVEL =====

// Função para definir ROI fixa (câmera em posição fixa)
Rect detectarAreaUtil(Mat& img) {
    int largura_img = img.cols;
    int altura_img = img.rows;
    
    // ROI FIXA - AJUSTE CONFORME NECESSÁRIO
    int x = largura_img * 0.15;      // 15% da esquerda
    int y = altura_img * 0.10;       // 10% do topo
    int largura = largura_img * 0.70; // 70% da largura
    int altura = altura_img * 0.80;   // 80% da altura
    
    Rect roi(x, y, largura, altura);
    
    // Garantir que está dentro da imagem
    roi.x = max(0, roi.x);
    roi.y = max(0, roi.y);
    roi.width = min(roi.width, img.cols - roi.x);
    roi.height = min(roi.height, img.rows - roi.y);
    
    return roi;
}

double detectarFeijoes(Mat& img, Rect roi) {
    Mat regiao = img(roi);
    
    Mat gray;
    cvtColor(regiao, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, gray, Size(5, 5), 0);
    
    Mat maskEscuro;
    threshold(gray, maskEscuro, 90, 255, THRESH_BINARY_INV);
    
    Mat hsv;
    cvtColor(regiao, hsv, COLOR_BGR2HSV);
    
    Mat maskPreto;
    inRange(hsv, Scalar(0, 0, 0), Scalar(180, 255, 110), maskPreto);
    
    Mat maskCombinada;
    bitwise_and(maskEscuro, maskPreto, maskCombinada);
    
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
    morphologyEx(maskCombinada, maskCombinada, MORPH_OPEN, kernel);
    morphologyEx(maskCombinada, maskCombinada, MORPH_CLOSE, kernel);
    
    double pixelsFeijao = countNonZero(maskCombinada);
    double totalPixels = roi.width * roi.height;
    
    return (pixelsFeijao / totalPixels) * 100.0;
}

double analisarNivel(string& statusOut) {
    // Capturar foto atual
    if (!capturarFoto("pote_atual.jpg")) {
        return -1;
    }
    
    Mat imgAtual = imread("pote_atual.jpg");
    if (imgAtual.empty()) {
        salvarLog("ERRO: Não foi possível ler pote_atual.jpg");
        return -1;
    }
    
    Rect roi = detectarAreaUtil(imgAtual);
    double densidadeAtual = detectarFeijoes(imgAtual, roi);
    
    double range = densidadeCheio - densidadeVazio;
    if (range <= 0) {
        salvarLog("ERRO: Calibração inválida!");
        return -1;
    }
    
    double nivel = ((densidadeAtual - densidadeVazio) / range) * 100.0;
    nivel = max(0.0, min(100.0, nivel));
    
    // Determinar status e cor
    string status;
    Scalar cor;
    
    if (nivel < 10) {
        status = "VAZIO";
        cor = Scalar(0, 0, 255);
    } else if (nivel < 25) {
        status = "MUITO BAIXO";
        cor = Scalar(0, 100, 255);
    } else if (nivel < 45) {
        status = "BAIXO";
        cor = Scalar(0, 165, 255);
    } else if (nivel < 65) {
        status = "MEDIO";
        cor = Scalar(0, 255, 255);
    } else if (nivel < 85) {
        status = "BOM";
        cor = Scalar(100, 255, 100);
    } else {
        status = "CHEIO";
        cor = Scalar(0, 255, 0);
    }
    
    statusOut = status;
    
        // ===== VISUALIZAÇÃO COMPLETA =====
    Mat resultado = imgAtual.clone();

    // Desenhar ROI na imagem principal
    rectangle(resultado, roi, Scalar(0, 255, 255), 2);

    // Criar painel superior preto
    int painelAltura = 120;
    Mat painel = Mat::zeros(painelAltura, resultado.cols, CV_8UC3);

    // Título
    putText(painel, "DETECTOR DE NIVEL DE RACAO",
            Point(20, 35), FONT_HERSHEY_SIMPLEX, 0.8,
            Scalar(255, 255, 255), 2);

    // Texto do nível
    string textoNivel = "Nivel: " + to_string((int)nivel) + "%";
    putText(painel, textoNivel,
            Point(20, 70), FONT_HERSHEY_SIMPLEX, 1.5,
            cor, 3);

    // Texto do status (CHEIO, BAIXO, VAZIO, ETC)
    putText(painel, status,
            Point(20, 105), FONT_HERSHEY_SIMPLEX, 0.9,
            cor, 2);

    // ===== BARRA DE PROGRESSO =====
    int barraX = resultado.cols - 250;
    int barraY = 50;
    int barraLargura = 200;
    int barraAltura = 30;

    // Fundo
    rectangle(painel,
              Point(barraX, barraY),
              Point(barraX + barraLargura, barraY + barraAltura),
              Scalar(80, 80, 80), FILLED);

    // Preenchimento
    int preenchimento = (barraLargura * nivel) / 100.0;
    rectangle(painel,
              Point(barraX, barraY),
              Point(barraX + preenchimento, barraY + barraAltura),
              cor, FILLED);

    // Borda da barra
    rectangle(painel,
              Point(barraX, barraY),
              Point(barraX + barraLargura, barraY + barraAltura),
              Scalar(255, 255, 255), 2);

    // Combinar painel + imagem original
    Mat final;
    vconcat(painel, resultado, final);

    // Salvar imagem final
    imwrite("resultado.jpg", final);
    
    return nivel;
}

// ===== CALIBRAÇÃO =====

bool calibrarSistema() {
    salvarLog("========================================");
    salvarLog("    CALIBRANDO A PARTIR DAS FOTOS EXISTENTES");
    salvarLog("========================================");
    
    // Carregar foto VAZIA existente
    Mat imgVazio = imread("vazio.jpg");
    if (imgVazio.empty()) {
        salvarLog("ERRO: Arquivo vazio.jpg não encontrado!");
        salvarLog("Por favor, coloque vazio.jpg na pasta.");
        return false;
    }
    
    Rect roiVazio = detectarAreaUtil(imgVazio);
    densidadeVazio = detectarFeijoes(imgVazio, roiVazio);
    salvarLog("Densidade VAZIO: " + to_string(densidadeVazio) + "%");
    
    // Carregar foto CHEIA existente
    Mat imgCheio = imread("cheio.jpg");
    if (imgCheio.empty()) {
        salvarLog("ERRO: Arquivo cheio.jpg não encontrado!");
        salvarLog("Por favor, coloque cheio.jpg na pasta.");
        return false;
    }
    
    Rect roiCheio = detectarAreaUtil(imgCheio);
    densidadeCheio = detectarFeijoes(imgCheio, roiCheio);
    salvarLog("Densidade CHEIO: " + to_string(densidadeCheio) + "%");
    
    if (densidadeCheio < densidadeVazio * 2) {
        salvarLog("AVISO: Diferença entre cheio e vazio pequena.");
        salvarLog("Vazio: " + to_string(densidadeVazio) + "%, Cheio: " + to_string(densidadeCheio) + "%");
    }
    
    // Salvar calibração
    ofstream config("calibracao.cfg");
    config << densidadeVazio << endl;
    config << densidadeCheio << endl;
    config.close();
    
    salvarLog("========================================");
    salvarLog("    CALIBRAÇÃO CONCLUÍDA COM SUCESSO!");
    salvarLog("========================================");
    
    return true;
}

bool carregarCalibracao() {
    ifstream config("calibracao.cfg");
    if (!config.is_open()) {
        return false;
    }
    
    config >> densidadeVazio;
    config >> densidadeCheio;
    config.close();
    
    salvarLog("Calibração carregada: Vazio=" + to_string(densidadeVazio) + 
              "%, Cheio=" + to_string(densidadeCheio) + "%");
    return true;
}

// ===== MAIN =====

int main() {
    salvarLog("========================================");
    salvarLog("  SISTEMA DE ALIMENTAÇÃO AUTOMÁTICA");
    salvarLog("  Intervalo: " + to_string(INTERVALO_VERIFICACAO/60) + " minutos");
    salvarLog("  Nível mínimo: " + to_string(NIVEL_MINIMO) + "%");
    salvarLog("========================================");
    
    // Inicializar CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Inicializar servo
    if (!inicializarServo()) {
        salvarLog("ERRO FATAL: Falha ao inicializar servo!");
        curl_global_cleanup();
        return -1;
    }
    
    // Carregar ou criar calibração
    if (!carregarCalibracao()) {
        salvarLog("Calibração não encontrada. Iniciando calibração...");
        if (!calibrarSistema()) {
            salvarLog("ERRO FATAL: Falha na calibração!");
            gpioTerminate();
            curl_global_cleanup();
            return -1;
        }
    }
    
    // Enviar mensagem de inicialização
    enviarMensagemTelegram("🚀 Sistema de alimentação inicializado!\n\n"
                          "✅ Servo conectado\n"
                          "✅ Câmera OK\n"
                          "✅ Calibração carregada\n\n"
                          "Monitoramento iniciado!");
    
    salvarLog("Sistema pronto! Iniciando monitoramento...");
    
    // Loop principal
    int ciclo = 0;
    while (true) {
        ciclo++;
        salvarLog("========== CICLO #" + to_string(ciclo) + " ==========");
        
        string status;
        double nivel = analisarNivel(status);
        
        if (nivel < 0) {
            salvarLog("ERRO: Falha ao analisar nível. Tentando novamente no próximo ciclo.");
            enviarMensagemTelegram("⚠️ ERRO: Falha ao verificar nível de ração!");
        } else {
            // Salvar histórico
            ofstream historico("historico.csv", ios::app);
            historico << obterTimestamp() << "," << (int)nivel << endl;
            historico.close();
            
            salvarLog("Nível atual: " + to_string((int)nivel) + "% - " + status);
            
            // Enviar relatório ao Telegram
            enviarRelatorioTelegram(nivel, status);
            
            // Verificar se precisa reabastecer
            if (nivel < NIVEL_MINIMO) {
                salvarLog("⚠️  ALERTA: Nível abaixo de " + to_string(NIVEL_MINIMO) + "%!");
                salvarLog("🔄 Iniciando reabastecimento...");
                
                enviarMensagemTelegram("🔄 Iniciando reabastecimento automático...");
                
                abrirPorta();
                fecharPorta();
                
                salvarLog("✅ Reabastecimento concluído!");
                
                // Aguardar ração se acomodar
                salvarLog("Aguardando 30 segundos para ração se acomodar...");
                this_thread::sleep_for(chrono::seconds(30));
                
                // Verificar novo nível
                string novoStatus;
                double novoNivel = analisarNivel(novoStatus);
                if (novoNivel > 0) {
                    salvarLog("📊 Novo nível: " + to_string((int)novoNivel) + "%");
                    
                    // Enviar relatório após reabastecimento
                    string msgReabastecido = "✅ REABASTECIMENTO CONCLUÍDO!\n\n"
                                            "📊 Nível anterior: " + to_string((int)nivel) + "%\n"
                                            "📊 Nível atual: " + to_string((int)novoNivel) + "%\n"
                                            "📌 Status: " + novoStatus + "\n"
                                            "🕐 " + obterTimestamp();
                    
                    enviarFotoTelegram("resultado.jpg", msgReabastecido);
                    
                    // Salvar reabastecimento no histórico
                    ofstream historico("historico.csv", ios::app);
                    historico << obterTimestamp() << "," << (int)novoNivel << ",REABASTECIDO" << endl;
                    historico.close();
                }
            } else {
                salvarLog("✓ Nível OK - Nenhuma ação necessária");
            }
        }
        
        // Aguardar próxima verificação
        salvarLog("Próxima verificação em " + to_string(INTERVALO_VERIFICACAO/60) + " minutos.");
        salvarLog("==========================================\n");
        
        this_thread::sleep_for(chrono::seconds(INTERVALO_VERIFICACAO));
    }
    
    // Finalizar
    gpioTerminate();
    curl_global_cleanup();
    
    return 0;
}
