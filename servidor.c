#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define PUERTO 8080
#define TAM 8192
static void enviar(SOCKET cliente, const char *tipo, const char *texto)
{
    // aqui se prepara y envia la respuesta http al cliente incluyendo el tipo de contenido y el texto de respuesta0
    // es como si el navegador dijera "hey servidor dame algo" y esta funcion dice "que onda, si aqui tienes"
}
static void atender(SOCKET cliente)
{

    // aqui leer la solicitud y atender formularios de clientes, pagos y archivos de la pagina
}
int main(void)
{

    // inisiar el servidor local y crear los archivos y esperar solicitudes del navegador
}
