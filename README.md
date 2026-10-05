# Control sencillo de pagos

Esquema inicial para un programa local de clientes y pagos, con una página para abrir en el navegador.

El servidor todavía no funciona: las funciones de `servidor.c` están vacías y tienen comentarios de lo que se deberá programar. `index.html` y `estilos.css` son el borrador de la pantalla.

## Partes del proyecto

- `servidor.c`: funciones pendientes para iniciar el servidor, atender solicitudes y responder al navegador.
- `index.html`: formularios sencillos para clientes y pagos.
- `estilos.css`: presentación básica de la página.

Cuando se programe el servidor, podrá compilarse en Windows con GCC:

```powershell
gcc servidor.c -o servidor.exe -lws2_32
```

Si ya tienes un `servidor.exe` de una versión anterior, no lo uses: no corresponde a estos esqueletos.
