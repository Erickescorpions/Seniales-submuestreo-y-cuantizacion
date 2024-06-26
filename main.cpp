#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <unistd.h>
#include <sys/wait.h>

#include <random>
#include <algorithm>
#include <iostream>

#define N 1000
#define PI M_PI
#define SNR_MAXIMO 3

float* genera_sen(int longitud, int amplitud, int frecuencia);
float* genera_cos(int longitud, int amplitud, int frecuencia);
void fn_a_archivo(float* fn, const char* nombre_archivo, int longitud);
void fn_entero_a_archivo(int* fn, const char* nombre_archivo, int longitud);
float calcular_potencia_fn(float* fn, int longitud);
float calcular_snr(float potencia_ruido, float potencia_fn);
void generar_ruido_en_fn(float* fn, int longitud);
float* obtener_submuestras(float* fn_muestreada, int longitud, int razon_submuestreo, int longitud_submuestreo);
int* cuantizacion(float* x, int longitud, int qi, int redondeo);
float calcular_error_cuantizacion(float* x_original, int* senial_cuantizada, int longitud, int longitud_palabra);

int main() {
    int numero_equipo;
    fputs("Ingresa tu numero de equipo: ", stdout);
    scanf("%d", &numero_equipo);

    int amplitud = 2 * numero_equipo;
    int frecuencia = 2 * numero_equipo;

    float* fn = NULL;
    int razon_submuestreo;
    int qi1 = 0;
    int qi2 = 0;

    if(numero_equipo % 2 == 0) { // par 
        fn = genera_cos(N, amplitud, frecuencia);
        razon_submuestreo = 2 * numero_equipo;
        qi1 = 5;
        qi2 = 12;
    } else { // impar
        fn = genera_sen(N, amplitud, frecuencia);
        razon_submuestreo = 3 * numero_equipo;
        qi1 = 6;
        qi2 = 13; 
    }

    generar_ruido_en_fn(fn, N);
    fn_a_archivo(fn, "fn.dat", N);
    
    int longitud_submuestreo = floor((N + razon_submuestreo) / razon_submuestreo);
    
    float* submuestras = obtener_submuestras(fn, N, razon_submuestreo, longitud_submuestreo);
    fn_a_archivo(submuestras, "submuestras.dat", longitud_submuestreo);

    // cuantizamos por redondeo 5 bits
    int* cuantizada_redondeo_qi1 = cuantizacion(fn, N, qi1, 1);
    // cuantizamos por truncamiento 5 bits
    int* cuantizada_truncamiento_qi1 = cuantizacion(fn, N, qi1, 0);

    fn_entero_a_archivo(cuantizada_redondeo_qi1, "redondeo_qi1.dat", N);
    fn_entero_a_archivo(cuantizada_truncamiento_qi1, "truncamiento_qi1.dat", N);

    // cuantizamos por redondeo 12 bits
    int* cuantizada_redondeo_qi2 = cuantizacion(fn, N, qi2, 1);
    // cuantizamos por truncamiento 12 bits
    int* cuantizada_truncamiento_qi2 = cuantizacion(fn, N, qi2, 0);

    fn_entero_a_archivo(cuantizada_redondeo_qi2, "redondeo_qi2.dat", N);
    fn_entero_a_archivo(cuantizada_truncamiento_qi2, "truncamiento_qi2.dat", N);

    puts("\nErrores de cuantizacion:\n");
    printf("El error de cuantizacion para %d bits con valores redondeados es: %f\n", qi1, calcular_error_cuantizacion(fn, cuantizada_redondeo_qi1, N, qi1));
    printf("El error de cuantizacion para %d bits con valores truncados es: %f\n", qi1, calcular_error_cuantizacion(fn, cuantizada_truncamiento_qi1, N, qi1));
    printf("El error de cuantizacion para %d bits con valores redondeados es: %f\n", qi2, calcular_error_cuantizacion(fn, cuantizada_redondeo_qi2, N, qi2));
    printf("El error de cuantizacion para %d bits con valores truncados es: %f\n", qi2, calcular_error_cuantizacion(fn, cuantizada_truncamiento_qi2, N, qi2));

    // creamos un proceso hijo con fork para lanzar la grafica de la senial original
    if(fork() == 0) {
        execlp("gnuplot", "gnuplot", "-p", "original.gp", NULL);
        perror("Error al ejecutar Gnuplot para original.gp");
        exit(1);
    }

    // proceso hijo para grafica de la senial submuestreada
    if(fork() == 0) {
        execlp("gnuplot", "gnuplot", "-p", "submuestreo.gp", NULL);
        perror("Error al ejecutar Gnuplot para submuestreo.gp");
        exit(1);
    }

    // proceso hijo para las graficas de cuantizacion
    if(fork() == 0) {
        // Este es el proceso hijo
        execlp("gnuplot", "gnuplot", "-p", "cuantizacion_qi1.gp", NULL);
        perror("Error al ejecutar Gnuplot para cuantizacion_qi1.gp");
        exit(1);
    }

    // proceso hijo para las graficas de cuantizacion qi2
    if(fork() == 0) {
        // Este es el proceso hijo
        execlp("gnuplot", "gnuplot", "-p", "cuantizacion_qi2.gp", NULL);
        perror("Error al ejecutar Gnuplot para cuantizacion_qi2.gp");
        exit(1);
    }

    // Esperar a que ambos procesos hijos terminen
    wait(NULL);
    wait(NULL);

    // liberamos memoria
    free(fn);
    free(submuestras);
    free(cuantizada_redondeo_qi1);
    free(cuantizada_truncamiento_qi1);
    free(cuantizada_truncamiento_qi2);
    free(cuantizada_redondeo_qi2);

    return 0;
}


float* genera_sen(int longitud, int amplitud, int frecuencia) {
    float* seno = (float*) malloc(longitud * sizeof(float));
    for(int n = 0; n < longitud; n++) {
        seno[n] = amplitud * sin(2 * PI * frecuencia * n / N);
    }

    return seno;
}

float* genera_cos(int longitud, int amplitud, int frecuencia) {
    float* coseno = (float*) malloc(longitud * sizeof(float));
    for(int n = 0; n < longitud; n++) {
        coseno[n] = amplitud * cos(2 * PI * frecuencia * n / N);
    }

    return coseno;
}

void fn_a_archivo(float* fn, const char* nombre_archivo, int longitud) {
    FILE* archivo = fopen(nombre_archivo, "w");

    for(int i = 0; i < longitud; i++) {
        fprintf(archivo, "%.4f\n", fn[i]);
        //printf("%f - %d\n", fn[i], i);
    }

    fclose(archivo);
}

void fn_entero_a_archivo(int* fn, const char* nombre_archivo, int longitud) {
    FILE* archivo = fopen(nombre_archivo, "w");

    for(int i = 0; i < longitud; i++) {
        fprintf(archivo, "%d\n", fn[i]);
        //printf("%f - %d\n", fn[i], i);
    }

    fclose(archivo);
}

float calcular_potencia_fn(float* fn, int longitud) {
    float potencia_fn = 0.0;

    for(int i = 0; i < longitud; i++) {
        potencia_fn += fn[i] * fn[i];
    }

    potencia_fn = potencia_fn / longitud;
    printf("La potencia de la senal es: %f\n", potencia_fn);

    return potencia_fn;
}

float calcular_snr(float potencia_ruido, float potencia_fn) {
    return 10 * log10(potencia_fn / potencia_ruido);
}

float calcular_potencia_ruido(float snr, float potencia_fn) {
    float potencia_ruido = 1; 
    float snr_calculado = calcular_snr(potencia_ruido, potencia_fn);
    while(snr_calculado >= snr) {
        potencia_ruido += 0.01;
        snr_calculado = calcular_snr(potencia_ruido, potencia_fn);
    }

    printf("La potencia del ruido es: %f\n", potencia_ruido);
    printf("El snr calculado es: %f \n", snr_calculado);
    return potencia_ruido;
}

// SNR = 10 * log10(Potencia de la senal / Potencia del ruido)
// No conocemos la potencia del ruido
// SNR < 3db
void generar_ruido_en_fn(float* fn, int longitud) {

    float potencia_fn = calcular_potencia_fn(fn, longitud);

    // Calculamos la potencia del ruido
    float potencia_ruido = calcular_potencia_ruido(SNR_MAXIMO, potencia_fn);
    float desviacion_estandar_ruido = sqrt(potencia_ruido);
    
    printf("La desviacion estandar del ruido es: %f\n", desviacion_estandar_ruido);

    std::default_random_engine generator;
    std::normal_distribution<double> distribution(0.0, desviacion_estandar_ruido);

    // Aplicamos el ruido a la senal 
    for(int i = 0; i < longitud; i++) {
        fn[i] = fn[i] + distribution(generator);
    }
}


float* obtener_submuestras(float* fn, int longitud, int razon_submuestreo, int longitud_submuestreo) {
    
    printf("La longitud de la señal submuestreada es: %d\n", longitud_submuestreo);
    float* submuestreo = (float*) malloc(longitud_submuestreo * sizeof(float));

    int indice_submuestreo = 0;

    // Recorre la señal y submuestrea cada razon_submuestreo
    for (int n = 0; n < longitud; n += razon_submuestreo) {
        submuestreo[indice_submuestreo] = fn[n];
        indice_submuestreo++;
    }

    return submuestreo;
}


int* cuantizacion(float* x, int longitud, int longitud_palabra, int redondeo) {
    int* senial_cuantizada = (int*) malloc(longitud * sizeof(int));
    
    // obtenemos la amplitud maxima de la señal
    float max_abs = fabs(x[0]);

    // Recorrer el array para encontrar el máximo
    for(int i = 1; i < longitud; ++i) {
        if(fabs(x[i]) > max_abs) {
            max_abs = fabs(x[i]);
        }
    }

    // si se activa la bandera de redondeo, se suma 0.5
    float aumento_por_redondeo = redondeo == 1 ? 0.5 : 0.0;

    // quitamos un bit para el signo
    int longitud_palabra_sin_signo = longitud_palabra - 1;

    // calculamos el numero maximo que podemos guardar en la cantidad de bits que nos dieron
    int maximo_valor = pow(2, longitud_palabra_sin_signo) - 1;
    printf("El rango para qi=%d bits es: %d \n", longitud_palabra, maximo_valor);

    for(int i = 0; i < N; i++) {
        // Normalizamos la muestra
        float muestra_normalizada = x[i] / max_abs;
        
        // cuantizamos nuestra muestra -> (int) muestra * 2 ^ (longitud palabra sin signo)
        // si queremos que el resultado se redondee, sumamos 0.5
        int valor_cuantizado = floor((muestra_normalizada * pow(2, longitud_palabra_sin_signo)) + aumento_por_redondeo);
        
        // revisamos el valor cuantizado no se pase del maximo que podemos guardar

        if(valor_cuantizado > maximo_valor) {
            // si se pasa, lo corregimos al maximo qeu se puede guardar
            valor_cuantizado = maximo_valor;
        }

        // al contar con numero negativos, revisamos que no se pase del minimo negativo
        if(valor_cuantizado < -maximo_valor) {
            // si se pasa, lo encogemos al minimo que se puede guardar
            valor_cuantizado = -maximo_valor; // Corregir rango si es menor que el mínimo
        }

        senial_cuantizada[i] = valor_cuantizado;
    }

    return senial_cuantizada;
}

float calcular_error_cuantizacion(float* x_original, int* senial_cuantizada, int longitud, int longitud_palabra) {
    float error_total = 0.0;

    // Normalizamos ambas seniales
    // obtenemos el maximo de la senial original
    // obtenemos la amplitud maxima de la señal
    float max_original = fabs(x_original[0]);

    // Recorrer el array para encontrar el máximo
    for(int i = 1; i < longitud; ++i) {
        if(fabs(x_original[i]) > max_original) {
            max_original = fabs(x_original[i]);
        }
    }

    // obtenemos el maximo de la senial cuantizada 
    // normalizamos
    float max_cuantizada = fabs(senial_cuantizada[0]);

    // Recorrer el array para encontrar el máximo
    for(int i = 1; i < longitud; ++i) {
        if(fabs(senial_cuantizada[i]) > max_cuantizada) {
            max_cuantizada = fabs(senial_cuantizada[i]);
        }
    }

    // Calcular el error cuadrático medio
    for(int i = 0; i < longitud; i++) {
        // normalizamos los vales antes
        float n_original = x_original[i] / max_original;
        float n_cuantizada = senial_cuantizada[i] / max_cuantizada;

        float error = n_original - n_cuantizada;
        error_total += error * error;
    }

    float rmse = sqrt(error_total / longitud);
    return rmse;
}