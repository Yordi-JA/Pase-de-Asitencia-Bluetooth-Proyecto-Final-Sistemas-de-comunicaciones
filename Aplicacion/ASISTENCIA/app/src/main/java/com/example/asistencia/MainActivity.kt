package com.example.asistencia

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothSocket
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.IOException
import java.util.*

class MainActivity : AppCompatActivity() {

    // Componentes de la Interfaz (UI)
    private lateinit var btnConnect: Button
    private lateinit var btnSend: Button
    private lateinit var etID: EditText
    private lateinit var tvLog: TextView

    // Componentes Bluetooth
    private val bluetoothAdapter: BluetoothAdapter? = BluetoothAdapter.getDefaultAdapter()
    private var bluetoothSocket: BluetoothSocket? = null
    // UUID Estándar para SPP
    private val myUUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Inicializar vistas
        btnConnect = findViewById(R.id.btnConnect)
        btnSend = findViewById(R.id.btnSend)
        etID = findViewById(R.id.etAccountID)
        tvLog = findViewById(R.id.tvLog)

        // 1. Lógica del Botón Conectar/Desconectar
        btnConnect.setOnClickListener {
            if (bluetoothSocket != null && bluetoothSocket!!.isConnected) {
                disconnect()
            } else {
                if (checkPermissions()) {
                    connectToDevice("Asistencia_Gpo05")
                } else {
                    log("Faltan permisos de Bluetooth.")
                }
            }
        }

        // 2. Lógica del Botón Enviar
        btnSend.setOnClickListener {
            val idInput = etID.text.toString().trim()
            if (idInput.isNotEmpty()) {
                sendAttendancePacket(idInput)
            } else {
                etID.error = "Ingrese ID"
            }
        }
    }

    // --- FUNCIONES DE BLUETOOTH ---

    private fun connectToDevice(deviceName: String) {
        log("Buscando $deviceName...")

        if (bluetoothAdapter == null) {
            log("Este dispositivo no soporta Bluetooth")
            return
        }

        if (!bluetoothAdapter.isEnabled) {
            log("Por favor enciende el Bluetooth")
            return
        }

        try {
            val pairedDevices = bluetoothAdapter.bondedDevices
            val device = pairedDevices.find { it.name == deviceName }

            if (device != null) {
                CoroutineScope(Dispatchers.IO).launch {
                    try {
                        try { bluetoothAdapter.cancelDiscovery() } catch (e: SecurityException) {}

                        bluetoothSocket = device.createRfcommSocketToServiceRecord(myUUID)
                        bluetoothSocket?.connect()

                        withContext(Dispatchers.Main) {
                            log("¡Conectado a ${device.name}!")
                            btnSend.isEnabled = true
                            btnConnect.text = "Desconectarse"
                            btnConnect.isEnabled = true
                            listenForData() // Activamos escucha
                        }
                    } catch (e: Exception) {
                        withContext(Dispatchers.Main) {
                            log("Error de conexión: ${e.message}")
                            try { bluetoothSocket?.close() } catch (e2: Exception) {}
                        }
                    }
                }
            } else {
                log("No se encontró '$deviceName'. Paréalo en ajustes primero.")
            }
        } catch (e: SecurityException) {
            log("Error de seguridad/permisos: ${e.message}")
        }
    }

    private fun listenForData() {
        CoroutineScope(Dispatchers.IO).launch {
            val buffer = ByteArray(1024)
            var bytes: Int

            while (bluetoothSocket != null && bluetoothSocket!!.isConnected) {
                try {
                    bytes = bluetoothSocket!!.inputStream.read(buffer)
                    if (bytes > 0) {
                        val incomingMessage = String(buffer, 0, bytes)
                        withContext(Dispatchers.Main) {
                            log("Sistema: $incomingMessage")
                        }
                    }
                } catch (e: IOException) {
                    break
                }
            }
        }
    }

    private fun disconnect() {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                bluetoothSocket?.close()
                bluetoothSocket = null
                withContext(Dispatchers.Main) {
                    btnConnect.text = "Conectarse"
                    btnConnect.isEnabled = true
                    btnSend.isEnabled = false
                    log("Desconectado.")
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    log("Error al desconectar: ${e.message}")
                }
            }
        }
    }

    private fun sendAttendancePacket(id: String) {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                if (bluetoothSocket != null && bluetoothSocket!!.isConnected) {

                    // --- CORRECCIÓN CRÍTICA DE DESBORDAMIENTO (32-bit compatible) ---

                    // 1. Obtenemos el tiempo absoluto (64 bits)
                    val absoluteTime = System.currentTimeMillis()

                    // 2. Aplicamos Módulo 10^9 para quedarnos con los últimos 9 dígitos.
                    //    Esto garantiza que el valor quepa en un 'unsigned long' de Arduino.
                    val t1Relative = absoluteTime % 1000000000L

                    // 3. Construcción del Paquete: [ID],[T1_Relativo]\n
                    val dataToSend = "$id\n"

                    // 4. Envío
                    bluetoothSocket!!.outputStream.write(dataToSend.toByteArray())

                    withContext(Dispatchers.Main) {
                        // Mostramos lo que realmente se envió para verificar
                        log("Enviado: $dataToSend")
                    }
                } else {
                    withContext(Dispatchers.Main) {
                        log("Error: No conectado.")
                        btnConnect.text = "Conectarse"
                        btnSend.isEnabled = false
                    }
                }
            } catch (e: IOException) {
                withContext(Dispatchers.Main) {
                    log("Error al enviar: ${e.message}")
                }
            }
        }
    }

    // --- UTILIDADES ---

    private fun log(message: String) {
        tvLog.append("\n> $message")
    }

    private fun checkPermissions(): Boolean {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val permissions = arrayOf(
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.BLUETOOTH_SCAN
            )
            var allGranted = true
            for (perm in permissions) {
                if (ActivityCompat.checkSelfPermission(this, perm) != PackageManager.PERMISSION_GRANTED) {
                    allGranted = false
                    break
                }
            }
            if (!allGranted) {
                ActivityCompat.requestPermissions(this, permissions, 1)
                return false
            }
        }
        return true
    }
}