🔐 M5 Cardputer TOTP Authenticator (Pro Version)
Convierte tu M5 Cardputer en un dispositivo de autenticación de dos factores (2FA) de hardware premium, seguro y completamente independiente. Una excelente alternativa física a Google Authenticator o Authy, equipada con inyección de código por USB, sincronización de tiempo global y protección por PIN.



✨ Características Principales
⌨️ Auto-Escritura por USB (HID): Conecta el Cardputer a tu PC y presiona la barra espaciadora para inyectar automáticamente el código de 6 dígitos en tu pantalla. ¡Adiós a teclear manualmente!
🔒 Seguridad de Doble Capa: Protegido por un PIN maestro. Al primer inicio te pedirá configurar tu propia clave, bloqueando el acceso a miradas indiscretas.

📡 Gestor Wi-Fi Visual y NTP (UTC): Escáner de redes integrado con reconexión automática. Sincroniza la hora exacta directamente desde servidores NTP globales (forzado a UTC para una precisión matemática perfecta de los tokens TOTP).

🔋 Ahorro Inteligente de Energía: La pantalla se apaga automáticamente tras 30 segundos de inactividad para conservar la batería al máximo. Se despierta al instante con cualquier tecla.

🧩 Decodificador Base32 Robusto: Acepta llaves secretas copiadas directamente de la web, ignorando espacios en blanco y caracteres de relleno de forma automática.



🚀 Primeros Pasos (First Setup)
El PIN de Fábrica: Al encender el Cardputer por primera vez, ingresa el PIN por defecto: 1234.

Crea tu PIN: El sistema detectará que es tu primer inicio y te pedirá crear y confirmar tu propio PIN permanente de 4 dígitos.

Conexión Wi-Fi: Selecciona tu red en el escáner visual e introduce la contraseña. Esto solo se hace una vez; el dispositivo recordará la red y sincronizará la hora automáticamente en los próximos reinicios.



🎮 Controles del Menú
N : Añadir una nueva cuenta (Nombre + Llave Base32).

; y . : Navegar hacia arriba/abajo por la lista de tus cuentas.

ENTER : Seleccionar una cuenta para ver el código dinámico de 6 dígitos.

DEL : Borrar la cuenta seleccionada.

ESPACIO : (Mientras ves el código de 6 dígitos) Enviar el código por USB directamente a tu computadora.



🔑 ¿Cómo obtengo la llave Base32 de mis cuentas?
Cuando actives la Verificación en 2 Pasos en servicios como Google, Microsoft, GitHub, etc., se te mostrará un código QR.
En lugar de escanearlo, busca el enlace que dice "¿No puedes escanear el código QR?" o "Configuración manual". El servicio te dará una cadena de texto larga (ej. abcd efgh ijkl mnop). Esa es tu llave Base32. Cópiala e introdúcela en tu Cardputer. ¡Puedes escribirla con o sin espacios!
Este proyecto está diseñado para la comodidad y la seguridad del día a día. Las llaves secretas se almacenan en la memoria flash interna (NVS) del ESP32, protegidas por el PIN de acceso al software. Si bien es seguro contra intrusos físicos casuales, no cuenta con cifrado militar a nivel de hardware para resistir ataques avanzados de extracción de chip. Se recomienda para cuentas de correo, redes sociales y plataformas de trabajo, pero evalúa su uso para billeteras de criptomonedas de muy alto valor.
