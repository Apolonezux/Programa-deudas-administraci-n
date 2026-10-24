#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define PUERTO 8080
#define TAM 8192

static int enviar_completo(SOCKET cliente, const char *datos, size_t longitud)
{
    size_t enviados = 0;

    while (enviados < longitud) {
        int resultado = send(cliente, datos + enviados, (int)(longitud - enviados), 0);
        if (resultado == SOCKET_ERROR || resultado == 0) {
            return 0;
        }
        enviados += (size_t)resultado;
    }
    return 1;
}

static void enviar_estado(SOCKET cliente, const char *estado, const char *tipo,
                          const char *texto)
{
    char cabecera[512];
    int longitud = (int)strlen(texto);
    int tam_cabecera = snprintf(cabecera, sizeof(cabecera),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "X-Content-Type-Options: nosniff\r\n\r\n",
        estado, tipo, longitud);

    if (tam_cabecera < 0 || (size_t)tam_cabecera >= sizeof(cabecera)) {
        return;
    }
    enviar_completo(cliente, cabecera, (size_t)tam_cabecera);
    enviar_completo(cliente, texto, (size_t)longitud);
}

static void enviar(SOCKET cliente, const char *tipo, const char *texto)
{
    enviar_estado(cliente, "200 OK", tipo, texto);
}

static int decodificar(const char *entrada, size_t longitud, char *salida,
                       size_t capacidad)
{
    size_t i = 0;
    size_t j = 0;

    while (i < longitud) {
        unsigned char caracter = (unsigned char)entrada[i++];
        if (caracter == '+') {
            caracter = ' ';
        } else if (caracter == '%') {
            int alto;
            int bajo;
            char a;
            char b;

            if (i + 1 >= longitud) {
                return 0;
            }
            a = entrada[i++];
            b = entrada[i++];
            alto = (a >= '0' && a <= '9') ? a - '0' :
                   (a >= 'A' && a <= 'F') ? a - 'A' + 10 :
                   (a >= 'a' && a <= 'f') ? a - 'a' + 10 : -1;
            bajo = (b >= '0' && b <= '9') ? b - '0' :
                   (b >= 'A' && b <= 'F') ? b - 'A' + 10 :
                   (b >= 'a' && b <= 'f') ? b - 'a' + 10 : -1;
            if (alto < 0 || bajo < 0) {
                return 0;
            }
            caracter = (unsigned char)((alto << 4) | bajo);
        }

        if (caracter == '\0' || caracter == '\r' || caracter == '\n' ||
            caracter == '|' || j + 1 >= capacidad) {
            return 0;
        }
        salida[j++] = (char)caracter;
    }
    salida[j] = '\0';
    return 1;
}

static int obtener_campo(const char *cuerpo, const char *nombre,
                         char *salida, size_t capacidad)
{
    const char *parte = cuerpo;
    size_t nombre_longitud = strlen(nombre);

    while (*parte != '\0') {
        const char *fin = strchr(parte, '&');
        const char *igual;
        size_t parte_longitud = fin != NULL ? (size_t)(fin - parte) : strlen(parte);

        igual = (const char *)memchr(parte, '=', parte_longitud);
        if (igual != NULL && (size_t)(igual - parte) == nombre_longitud &&
            memcmp(parte, nombre, nombre_longitud) == 0) {
            return decodificar(igual + 1, parte_longitud - nombre_longitud - 1,
                               salida, capacidad) && salida[0] != '\0';
        }
        if (fin == NULL) {
            break;
        }
        parte = fin + 1;
    }
    return 0;
}

static int numero_positivo(const char *texto)
{
    char *fin;
    double valor;

    errno = 0;
    valor = strtod(texto, &fin);
    return errno == 0 && fin != texto && *fin == '\0' &&
           isfinite(valor) && valor > 0;
}

static int fecha_valida(const char *texto)
{
    int anio;
    int mes;
    int dia;
    int dias_mes;
    int bisiesto;

    if (strlen(texto) != 10 || texto[4] != '-' || texto[7] != '-' ||
        sscanf(texto, "%4d-%2d-%2d", &anio, &mes, &dia) != 3) {
        return 0;
    }
    if (texto[0] < '0' || texto[0] > '9' ||
        texto[1] < '0' || texto[1] > '9' ||
        texto[2] < '0' || texto[2] > '9' ||
        texto[3] < '0' || texto[3] > '9' ||
        texto[5] < '0' || texto[5] > '9' ||
        texto[6] < '0' || texto[6] > '9' ||
        texto[8] < '0' || texto[8] > '9' ||
        texto[9] < '0' || texto[9] > '9' ||
        mes < 1 || mes > 12) {
        return 0;
    }
    bisiesto = anio % 4 == 0 && (anio % 100 != 0 || anio % 400 == 0);
    dias_mes = mes == 2 ? 28 + bisiesto :
               (mes == 4 || mes == 6 || mes == 9 || mes == 11) ? 30 : 31;
    return dia >= 1 && dia <= dias_mes;
}

static int registro_contiene(const char *archivo, int indice_campo,
                             const char *valor)
{
    FILE *entrada = fopen(archivo, "r");
    char linea[2048];

    if (entrada == NULL) {
        return 0;
    }
    while (fgets(linea, sizeof(linea), entrada) != NULL) {
        char *campos[4];
        char *cursor = linea;
        size_t longitud = strlen(linea);
        int i;
        int valido = 1;

        while (longitud > 0 &&
               (linea[longitud - 1] == '\n' || linea[longitud - 1] == '\r')) {
            linea[--longitud] = '\0';
        }
        campos[0] = cursor;
        for (i = 0; i < 3; ++i) {
            char *separador = strstr(cursor, " | ");
            if (separador == NULL) {
                valido = 0;
                break;
            }
            *separador = '\0';
            cursor = separador + 3;
            campos[i + 1] = cursor;
        }
        if (valido && strcmp(campos[indice_campo], valor) == 0) {
            fclose(entrada);
            return 1;
        }
    }
    fclose(entrada);
    return 0;
}

static int anexar_literal(char *salida, size_t capacidad, size_t *usados,
                          const char *texto)
{
    size_t longitud = strlen(texto);
    if (*usados + longitud + 1 >= capacidad) {
        return 0;
    }
    memcpy(salida + *usados, texto, longitud);
    *usados += longitud;
    salida[*usados] = '\0';
    return 1;
}

static int anexar_json(char *salida, size_t capacidad, size_t *usados,
                       const char *texto)
{
    const unsigned char *cursor = (const unsigned char *)texto;

    if (*usados + 1 >= capacidad) {
        return 0;
    }
    salida[(*usados)++] = '"';
    while (*cursor != '\0') {
        char escapado[7];
        size_t longitud;
        if (*cursor == '"' || *cursor == '\\') {
            escapado[0] = '\\';
            escapado[1] = (char)*cursor;
            longitud = 2;
        } else if (*cursor < 0x20) {
            int escrito = snprintf(escapado, sizeof(escapado), "\\u%04x", *cursor);
            if (escrito != 6) {
                return 0;
            }
            longitud = 6;
        } else {
            escapado[0] = (char)*cursor;
            longitud = 1;
        }
        if (*usados + longitud + 2 >= capacidad) {
            return 0;
        }
        memcpy(salida + *usados, escapado, longitud);
        *usados += longitud;
        ++cursor;
    }
    salida[(*usados)++] = '"';
    salida[*usados] = '\0';
    return 1;
}

static void servir_registros_json(SOCKET cliente, const char *nombre,
                                  int clientes)
{
    FILE *archivo = fopen(nombre, "rb");
    long tamano;
    size_t capacidad;
    size_t usados = 0;
    char *json;
    char linea[2048];
    int primero = 1;

    if (archivo == NULL) {
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "No se pudo abrir el archivo de registros.");
        return;
    }
    if (fseek(archivo, 0, SEEK_END) != 0 ||
        (tamano = ftell(archivo)) < 0 ||
        fseek(archivo, 0, SEEK_SET) != 0) {
        fclose(archivo);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "No se pudo leer el archivo de registros.");
        return;
    }
    if ((size_t)tamano > ((size_t)-1 - 256) / 8) {
        fclose(archivo);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "El archivo de registros es demasiado grande.");
        return;
    }
    capacidad = (size_t)tamano * 8 + 256;
    json = (char *)malloc(capacidad);
    if (json == NULL) {
        fclose(archivo);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "Memoria insuficiente.");
        return;
    }
    json[usados++] = '[';
    json[usados] = '\0';
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        char *campos[4];
        char *cursor = linea;
        size_t longitud = strlen(linea);
        int i;
        int valido = 1;

        while (longitud > 0 &&
               (linea[longitud - 1] == '\n' || linea[longitud - 1] == '\r')) {
            linea[--longitud] = '\0';
        }
        if (longitud == 0) {
            continue;
        }
        campos[0] = cursor;
        for (i = 0; i < 3; ++i) {
            char *separador = strstr(cursor, " | ");
            if (separador == NULL) {
                valido = 0;
                break;
            }
            *separador = '\0';
            cursor = separador + 3;
            campos[i + 1] = cursor;
        }
        if (!valido) {
            free(json);
            fclose(archivo);
            enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                          "Hay un registro con formato invalido.");
            return;
        }
        if (!primero) {
            if (usados + 1 >= capacidad) {
                free(json);
                fclose(archivo);
                enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                              "El archivo de registros excede el espacio disponible.");
                return;
            }
            json[usados++] = ',';
            json[usados] = '\0';
        }
        primero = 0;
        if (clientes) {
            if (!anexar_literal(json, capacidad, &usados,
                                "{\"name\":") ||
                !anexar_json(json, capacidad, &usados, campos[0]) ||
                !anexar_literal(json, capacidad, &usados, ",\"contact\":") ||
                !anexar_json(json, capacidad, &usados, campos[1]) ||
                !anexar_literal(json, capacidad, &usados, ",\"lot\":") ||
                !anexar_json(json, capacidad, &usados, campos[2]) ||
                !anexar_literal(json, capacidad, &usados, ",\"initialBalance\":") ||
                !anexar_json(json, capacidad, &usados, campos[3]) ||
                !anexar_literal(json, capacidad, &usados, "}")) {
                free(json);
                fclose(archivo);
                enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                              "No se pudo crear la respuesta de clientes.");
                return;
            }
        } else if (!anexar_literal(json, capacidad, &usados,
                                   "{\"name\":") ||
                   !anexar_json(json, capacidad, &usados, campos[0]) ||
                   !anexar_literal(json, capacidad, &usados, ",\"date\":") ||
                   !anexar_json(json, capacidad, &usados, campos[1]) ||
                   !anexar_literal(json, capacidad, &usados, ",\"amount\":") ||
                   !anexar_json(json, capacidad, &usados, campos[2]) ||
                   !anexar_literal(json, capacidad, &usados, ",\"folio\":") ||
                   !anexar_json(json, capacidad, &usados, campos[3]) ||
                   !anexar_literal(json, capacidad, &usados, "}")) {
            free(json);
            fclose(archivo);
            enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                          "No se pudo crear la respuesta de pagos.");
            return;
        }
    }
    fclose(archivo);
    if (usados + 2 > capacidad) {
        free(json);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "No se pudo completar la respuesta.");
        return;
    }
    json[usados++] = ']';
    json[usados] = '\0';
    enviar(cliente, "application/json", json);
    free(json);
}

static int guardar_registro(const char *archivo, const char *formato,
                            const char *a, const char *b, const char *c,
                            const char *d)
{
    FILE *salida = fopen(archivo, "a");
    int correcto;

    if (salida == NULL) {
        return 0;
    }
    correcto = fprintf(salida, formato, a, b, c, d) >= 0;
    if (fclose(salida) != 0) {
        correcto = 0;
    }
    return correcto;
}

static void procesar_formulario(SOCKET cliente, const char *ruta,
                                const char *cuerpo)
{
    char uno[256];
    char dos[256];
    char tres[256];
    char cuatro[256];
    const char *mensaje =
        "<!doctype html><html lang=\"es\"><meta charset=\"utf-8\">"
        "<title>Solicitud invalida</title><body>"
        "<p>Los datos no son validos o falta algun campo.</p>"
        "<p><a href=\"/\">Volver</a></p></body></html>";

    if (strcmp(ruta, "/cliente") == 0) {
        if (!obtener_campo(cuerpo, "nombre", uno, sizeof(uno)) ||
            !obtener_campo(cuerpo, "contacto", dos, sizeof(dos)) ||
            !obtener_campo(cuerpo, "terreno", tres, sizeof(tres)) ||
            !obtener_campo(cuerpo, "saldo", cuatro, sizeof(cuatro)) ||
            !numero_positivo(cuatro)) {
            enviar_estado(cliente, "400 Bad Request", "text/html", mensaje);
            return;
        }
        if (registro_contiene("clientes.txt", 0, uno)) {
            enviar_estado(cliente, "409 Conflict", "text/plain",
                          "Ya existe un cliente con ese nombre.");
            return;
        }
        if (!guardar_registro("clientes.txt", "%s | %s | %s | %s\n",
                              uno, dos, tres, cuatro)) {
            enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                          "No se pudo guardar el cliente.");
            return;
        }
    } else if (strcmp(ruta, "/pago") == 0) {
        if (!obtener_campo(cuerpo, "cliente", uno, sizeof(uno)) ||
            !obtener_campo(cuerpo, "fecha", dos, sizeof(dos)) ||
            !obtener_campo(cuerpo, "monto", tres, sizeof(tres)) ||
            !obtener_campo(cuerpo, "folio", cuatro, sizeof(cuatro)) ||
            !numero_positivo(tres) || !fecha_valida(dos)) {
            enviar_estado(cliente, "400 Bad Request", "text/html", mensaje);
            return;
        }
        if (!registro_contiene("clientes.txt", 0, uno)) {
            enviar_estado(cliente, "400 Bad Request", "text/plain",
                          "Selecciona un cliente registrado.");
            return;
        }
        if (registro_contiene("pagos.txt", 3, cuatro)) {
            enviar_estado(cliente, "409 Conflict", "text/plain",
                          "Ya existe un pago con ese folio.");
            return;
        }
        if (!guardar_registro("pagos.txt", "%s | %s | %s | %s\n",
                              uno, dos, tres, cuatro)) {
            enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                          "No se pudo guardar el pago.");
            return;
        }
    } else {
        enviar_estado(cliente, "404 Not Found", "text/plain",
                      "La ruta solicitada no existe.");
        return;
    }

    enviar(cliente, "text/html",
        "<!doctype html><html lang=\"es\"><meta charset=\"utf-8\">"
        "<title>Guardado</title><body><p>Registro guardado correctamente.</p>"
        "<p><a href=\"/\">Volver al inicio</a></p></body></html>");
}

static void servir_archivo(SOCKET cliente, const char *ruta)
{
    const char *nombre;
    const char *tipo;
    FILE *archivo;
    long tamano;
    char *contenido;
    size_t leidos;

    if (strcmp(ruta, "/") == 0 || strcmp(ruta, "/index.html") == 0) {
        nombre = "index.html";
        tipo = "text/html";
    } else if (strcmp(ruta, "/app.js") == 0) {
        nombre = "app.js";
        tipo = "application/javascript";
    } else if (strcmp(ruta, "/estilos.css") == 0) {
        nombre = "estilos.css";
        tipo = "text/css";
    } else if (strcmp(ruta, "/clientes.txt") == 0) {
        nombre = "clientes.txt";
        tipo = "text/plain";
    } else if (strcmp(ruta, "/pagos.txt") == 0) {
        nombre = "pagos.txt";
        tipo = "text/plain";
    } else {
        enviar_estado(cliente, "404 Not Found", "text/plain",
                      "El archivo solicitado no existe.");
        return;
    }

    archivo = fopen(nombre, "rb");
    if (archivo == NULL) {
        enviar_estado(cliente, "404 Not Found", "text/plain",
                      "No se encontro el archivo solicitado.");
        return;
    }
    if (fseek(archivo, 0, SEEK_END) != 0 ||
        (tamano = ftell(archivo)) < 0 ||
        fseek(archivo, 0, SEEK_SET) != 0) {
        fclose(archivo);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "No se pudo leer el archivo.");
        return;
    }
    contenido = (char *)malloc((size_t)tamano + 1);
    if (contenido == NULL) {
        fclose(archivo);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "Memoria insuficiente.");
        return;
    }
    leidos = fread(contenido, 1, (size_t)tamano, archivo);
    fclose(archivo);
    if (leidos != (size_t)tamano) {
        free(contenido);
        enviar_estado(cliente, "500 Internal Server Error", "text/plain",
                      "No se pudo leer el archivo.");
        return;
    }
    contenido[leidos] = '\0';
    enviar(cliente, tipo, contenido);
    free(contenido);
}

static void atender(SOCKET cliente)
{
    char solicitud[TAM];
    char metodo[8];
    char ruta[256];
    char *fin_cabecera = NULL;
    size_t usados = 0;
    size_t longitud_cuerpo = 0;

    while (usados < sizeof(solicitud) - 1) {
        int recibidos = recv(cliente, solicitud + usados,
                             (int)(sizeof(solicitud) - 1 - usados), 0);
        if (recibidos <= 0) {
            return;
        }
        usados += (size_t)recibidos;
        solicitud[usados] = '\0';

        if (fin_cabecera == NULL) {
            fin_cabecera = strstr(solicitud, "\r\n\r\n");
            if (fin_cabecera != NULL) {
                char *contenido_longitud = strstr(solicitud, "Content-Length:");
                if (contenido_longitud != NULL && contenido_longitud < fin_cabecera) {
                    char *fin_numero;
                    long valor;

                    contenido_longitud += strlen("Content-Length:");
                    while (*contenido_longitud == ' ' || *contenido_longitud == '\t') {
                        ++contenido_longitud;
                    }
                    valor = strtol(contenido_longitud, &fin_numero, 10);
                    if (fin_numero == contenido_longitud || valor < 0 ||
                        valor > (long)(sizeof(solicitud) - 1)) {
                        enviar_estado(cliente, "413 Content Too Large", "text/plain",
                                      "La solicitud es demasiado grande.");
                        return;
                    }
                    longitud_cuerpo = (size_t)valor;
                }
            }
        }
        if (fin_cabecera != NULL &&
            usados >= (size_t)(fin_cabecera - solicitud) + 4 + longitud_cuerpo) {
            break;
        }
    }

    if (fin_cabecera == NULL ||
        usados < (size_t)(fin_cabecera - solicitud) + 4 + longitud_cuerpo) {
        enviar_estado(cliente, "400 Bad Request", "text/plain",
                      "Solicitud HTTP incompleta o demasiado grande.");
        return;
    }
    if (sscanf(solicitud, "%7s %255s", metodo, ruta) != 2) {
        enviar_estado(cliente, "400 Bad Request", "text/plain",
                      "Solicitud HTTP invalida.");
        return;
    }
    if (strchr(ruta, '?') != NULL) {
        *strchr(ruta, '?') = '\0';
    }

    if (strcmp(metodo, "GET") == 0) {
        if (strcmp(ruta, "/api/clientes") == 0) {
            servir_registros_json(cliente, "clientes.txt", 1);
        } else if (strcmp(ruta, "/api/pagos") == 0) {
            servir_registros_json(cliente, "pagos.txt", 0);
        } else {
            servir_archivo(cliente, ruta);
        }
    } else if (strcmp(metodo, "POST") == 0) {
        if (strcmp(ruta, "/cliente") != 0 && strcmp(ruta, "/pago") != 0) {
            enviar_estado(cliente, "404 Not Found", "text/plain",
                          "La ruta solicitada no existe.");
            return;
        }
        procesar_formulario(cliente, ruta,
                            fin_cabecera + 4);
    } else {
        enviar_estado(cliente, "405 Method Not Allowed", "text/plain",
                      "Metodo HTTP no permitido.");
    }
}

int main(void)
{
    WSADATA datos;
    SOCKET servidor;
    struct sockaddr_in direccion;

    if (WSAStartup(MAKEWORD(2, 2), &datos) != 0) {
        fprintf(stderr, "No se pudo iniciar Winsock.\n");
        return EXIT_FAILURE;
    }

    servidor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servidor == INVALID_SOCKET) {
        fprintf(stderr, "No se pudo crear el socket: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    direccion.sin_port = htons(PUERTO);

    if (bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) == SOCKET_ERROR) {
        fprintf(stderr, "No se pudo usar el puerto %d: %d\n",
                PUERTO, WSAGetLastError());
        closesocket(servidor);
        WSACleanup();
        return EXIT_FAILURE;
    }
    if (listen(servidor, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "No se pudo iniciar la escucha: %d\n", WSAGetLastError());
        closesocket(servidor);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("Servidor iniciado en http://localhost:%d\n", PUERTO);
    for (;;) {
        SOCKET cliente = accept(servidor, NULL, NULL);
        if (cliente == INVALID_SOCKET) {
            fprintf(stderr, "Error al aceptar conexion: %d\n", WSAGetLastError());
            closesocket(servidor);
            WSACleanup();
            return EXIT_FAILURE;
        }
        atender(cliente);
        shutdown(cliente, SD_SEND);
        closesocket(cliente);
    }
}
