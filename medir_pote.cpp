#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

// Função para detectar a área útil do pote (ignora as bordas escuras)
Rect detectarAreaUtil(Mat& img) {
    // ROI mais conservadora - ignora bordas onde ficam sombras
    int margem_x = img.cols * 0.15;  // 15% de margem
    int margem_y = img.rows * 0.15;
    
    int largura = img.cols - 2 * margem_x;
    int altura = img.rows - 2 * margem_y;
    
    return Rect(margem_x, margem_y, largura, altura);
}

// Função melhorada para detectar feijões (ignora sombras)
double detectarFeijoes(Mat& img, Rect roi) {
    // Extrair região de interesse
    Mat regiao = img(roi);
    
    // Converter para escala de cinza para análise simples
    Mat gray;
    cvtColor(regiao, gray, COLOR_BGR2GRAY);
    
    // Aplicar blur para reduzir ruído
    GaussianBlur(gray, gray, Size(5, 5), 0);
    
    // Detectar pixels escuros (feijões) - threshold mais permissivo
    Mat maskEscuro;
    threshold(gray, maskEscuro, 100, 255, THRESH_BINARY_INV);
    
    // Converter para HSV para filtrar melhor
    Mat hsv;
    cvtColor(regiao, hsv, COLOR_BGR2HSV);
    
    // Detectar cores escuras e neutras (feijões pretos)
    // V < 120 (valor/brilho baixo)
    Mat maskPreto;
    inRange(hsv, Scalar(0, 0, 0), Scalar(180, 255, 120), maskPreto);
    
    // Combinar as duas máscaras (AND lógico)
    Mat maskCombinada;
    bitwise_and(maskEscuro, maskPreto, maskCombinada);
    
    // Remover pequenos ruídos
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
    morphologyEx(maskCombinada, maskCombinada, MORPH_OPEN, kernel);
    
    // Fechar pequenos buracos nos feijões
    morphologyEx(maskCombinada, maskCombinada, MORPH_CLOSE, kernel);
    
    // Salvar máscara para debug
    imwrite("mask_debug.jpg", maskCombinada);
    
    // Contar pixels de feijão
    double pixelsFeijao = countNonZero(maskCombinada);
    double totalPixels = roi.width * roi.height;
    
    return (pixelsFeijao / totalPixels) * 100.0;
}

int main() {
    cout << "=== CALIBRANDO SISTEMA ===\n\n";
    
    // ===== PROCESSAR IMAGEM VAZIA =====
    Mat imgVazio = imread("vazio.jpg");
    if (imgVazio.empty()) {
        cerr << "Erro ao carregar vazio.jpg\n";
        return -1;
    }
    
    Rect roiVazio = detectarAreaUtil(imgVazio);
    double densidadeVazio = detectarFeijoes(imgVazio, roiVazio);
    
    cout << "Densidade VAZIO: " << densidadeVazio << "%\n";
    
    // ===== PROCESSAR IMAGEM CHEIA =====
    Mat imgCheio = imread("cheio.jpg");
    if (imgCheio.empty()) {
        cerr << "Erro ao carregar cheio.jpg\n";
        return -1;
    }
    
    Rect roiCheio = detectarAreaUtil(imgCheio);
    double densidadeCheio = detectarFeijoes(imgCheio, roiCheio);
    
    cout << "Densidade CHEIO: " << densidadeCheio << "%\n";
    
    // Validação: cheio deve ter MUITO mais densidade que vazio
    if (densidadeCheio < densidadeVazio * 3) {
        cout << "\n*** AVISO: Diferença entre cheio e vazio muito pequena! ***\n";
        cout << "Verifique se as imagens estão corretas.\n\n";
    }
    
    // ===== PROCESSAR IMAGEM ATUAL =====
    Mat imgAtual = imread("pote.jpg");
    if (imgAtual.empty()) {
        cerr << "Erro ao carregar pote.jpg\n";
        return -1;
    }
    
    Rect roiAtual = detectarAreaUtil(imgAtual);
    double densidadeAtual = detectarFeijoes(imgAtual, roiAtual);
    
    cout << "Densidade ATUAL: " << densidadeAtual << "%\n";
    
    // ===== CALCULAR NÍVEL =====
    // Normalizar entre vazio e cheio
    double range = densidadeCheio - densidadeVazio;
    
    if (range <= 0) {
        cerr << "\nERRO: Imagem 'cheio' não tem mais feijões que 'vazio'!\n";
        return -1;
    }
    
    double nivel = ((densidadeAtual - densidadeVazio) / range) * 100.0;
    
    // Limitar entre 0 e 100
    nivel = max(0.0, min(100.0, nivel));
    
    cout << "\n========================================\n";
    cout << "       NÍVEL DE RAÇÃO: " << (int)nivel << "%\n";
    cout << "========================================\n";
    
    // Classificação com mais granularidade
    string status;
    Scalar cor;
    
    if (nivel < 10) {
        status = "VAZIO - REABASTECER URGENTE!";
        cor = Scalar(0, 0, 255); // Vermelho
    } else if (nivel < 25) {
        status = "MUITO BAIXO";
        cor = Scalar(0, 100, 255); // Vermelho-laranja
    } else if (nivel < 45) {
        status = "BAIXO";
        cor = Scalar(0, 165, 255); // Laranja
    } else if (nivel < 65) {
        status = "MEDIO";
        cor = Scalar(0, 255, 255); // Amarelo
    } else if (nivel < 85) {
        status = "BOM";
        cor = Scalar(100, 255, 100); // Verde claro
    } else {
        status = "CHEIO";
        cor = Scalar(0, 255, 0); // Verde
    }
    
    cout << "Status: " << status << "\n\n";
    
    // ===== VISUALIZAÇÃO =====
    Mat resultado = imgAtual.clone();
    
    // Desenhar ROI
    rectangle(resultado, roiAtual, Scalar(0, 255, 255), 2);
    
    // Criar painel de informações
    int painelAltura = 120;
    Mat painel = Mat::zeros(painelAltura, resultado.cols, CV_8UC3);
    
    // Adicionar informações ao painel
    putText(painel, "DETECTOR DE NIVEL DE RACAO", 
            Point(20, 35), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
    
    string textoNivel = "Nivel: " + to_string((int)nivel) + "%";
    putText(painel, textoNivel, 
            Point(20, 70), FONT_HERSHEY_SIMPLEX, 1.5, cor, 3);
    
    putText(painel, status, 
            Point(20, 105), FONT_HERSHEY_SIMPLEX, 0.9, cor, 2);
    
    // Adicionar barra de progresso
    int barraX = resultado.cols - 250;
    int barraY = 50;
    int barraLargura = 200;
    int barraAltura = 30;
    
    // Fundo da barra
    rectangle(painel, Point(barraX, barraY), 
              Point(barraX + barraLargura, barraY + barraAltura),
              Scalar(80, 80, 80), FILLED);
    
    // Preenchimento da barra
    int preenchimento = (barraLargura * nivel) / 100.0;
    rectangle(painel, Point(barraX, barraY), 
              Point(barraX + preenchimento, barraY + barraAltura),
              cor, FILLED);
    
    // Borda da barra
    rectangle(painel, Point(barraX, barraY), 
              Point(barraX + barraLargura, barraY + barraAltura),
              Scalar(255, 255, 255), 2);
    
    // Combinar painel com imagem
    Mat final;
    vconcat(painel, resultado, final);
    
    imwrite("resultado.jpg", final);
    
    cout << "Arquivos gerados:\n";
    cout << "  ✓ resultado.jpg - Visualização completa\n";
    cout << "  ✓ mask_debug.jpg - Máscara de detecção\n\n";
    
    return 0;
}