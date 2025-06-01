import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/services.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:todolist/data/Door_items.dart';

Future<void> setupSecureContext(MqttServerClient client) async {
  final ByteData rootCA = await rootBundle.load('assets/RootCA.pem');
  final ByteData deviceCert = await rootBundle.load(
    'assets/certificate.pem.crt',
  );
  final ByteData privateKey = await rootBundle.load('assets/private.pem.key');

  final SecurityContext context = SecurityContext.defaultContext;
  context.setClientAuthoritiesBytes(rootCA.buffer.asUint8List());
  context.useCertificateChainBytes(deviceCert.buffer.asUint8List());
  context.usePrivateKeyBytes(privateKey.buffer.asUint8List());

  client.securityContext = context;
}

Future<MqttServerClient> mqttConnect(
  String clientId,
  Function setState,
  Door_items door,
) async {
  final client = MqttServerClient.withPort(
    'a2cuiohotiqavz-ats.iot.ap-southeast-2.amazonaws.com',
    clientId,
    8883,
  );
  client.secure = true;
  client.logging(on: true);
  client.keepAlivePeriod = 20;

  client.onConnected = () => print('MQTT Connected');
  client.onDisconnected = () => print(' MQTT Disconnected');
  client.pongCallback = () => print(' Ping response');
  await setupSecureContext(client);

  final connMessage = MqttConnectMessage()
      .withClientIdentifier(clientId)
      .startClean();

  client.connectionMessage = connMessage;

  try {
    await client.connect();
  } catch (e) {
    print(' Connection failed: $e');
    return client;
  }

  if (client.connectionStatus?.state == MqttConnectionState.connected) {
    print(' Connected to MQTT broker');
  } else {
    print(' Connection failed with status: ${client.connectionStatus?.state}');
    return client;
  }

  // Subcribe topic
  const topic = 'raspi/data';
  client.subscribe(topic, MqttQos.atMostOnce);

  client.updates!.listen((List<MqttReceivedMessage<MqttMessage>> event) {
    final message = event[0].payload as MqttPublishMessage;
    final payload = MqttPublishPayload.bytesToStringAsString(
      message.payload.message,
    );
    handleMqttMessage(payload, door, setState);
  });
  return client;
}

void handleMqttMessage(String payload, Door_items door, Function setState) {
  try {
    final Map<String, dynamic> msg = jsonDecode(payload);

    setState(() {
      if (msg.containsKey("door state")) {
        if (msg["door state"] == 1) {
          door.Door_state_set = true;
        } else {
          door.Door_state_set = false;
        }
      }

      if (msg.containsKey("password")) {
        door.Door_password_set = msg["password"];
      }
    });

    print("🔓 Door State: ${door.Door_state_get()}");
    print("🔐 Password: ${door.Door_password_get()}");
  } catch (e) {
    print(" Error decoding MQTT message: $e");
  }
}

void publishPassword(MqttServerClient client, String head, String password) {
  final builder = MqttClientPayloadBuilder();
  final message = jsonEncode({head: password});
  builder.addString(message);
  client.publishMessage("raspi/control", MqttQos.atMostOnce, builder.payload!);

  print("Published: $message");
}
