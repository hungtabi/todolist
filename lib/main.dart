import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:todolist/mqtt.dart';
import 'package:flutter/material.dart';
import 'package:todolist/Door_control.dart';
import 'package:todolist/Door_state.dart';
import 'package:todolist/data/Door_items.dart';
import 'package:todolist/text_button.dart';

void main() {
  runApp(MaterialApp(debugShowCheckedModeBanner: false, home: MyApp()));
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});
  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  Door_items door = Door_items(password: '123456', door_state: true);
  MqttServerClient? client;
  @override
  void initState() {
    super.initState();
    mqttConnect("tabiClient", setState, door).then((c) {
      client = c;
      print("✅ MQTT connected");
      setState(() {}); // Cập nhật UI khi client đã sẵn sàng
    });
  }

  void rebuild() {
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      resizeToAvoidBottomInset: true,
      appBar: AppBar(
        backgroundColor: Color.fromARGB(255, 238, 6, 6),
        title: Text('SMART DOOR'),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(50),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.start,
          children: [
            Door_control(context, rebuild, door, client),
            SizedBox(height: 50),
            Door_state(door),
            text_button(context, door, client),
          ],
        ),
      ),
    );
  }
}
