import 'package:flutter/material.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:todolist/Iconbutton.dart';
import 'package:todolist/data/Door_items.dart';

Container Door_control(
  BuildContext context,
  Function rebuild,
  Door_items items,
  MqttServerClient? client,
) {
  return Container(
    decoration: BoxDecoration(
      color: const Color.fromARGB(255, 238, 6, 6),
      borderRadius: BorderRadius.circular(100),
      boxShadow: [
        BoxShadow(
          color: Color.fromARGB(80, 255, 0, 0), // đỏ nhạt
          offset: Offset(6, 0), // bóng sang phải
          blurRadius: 40,
          spreadRadius: 1,
        ),
        BoxShadow(
          color: Color.fromARGB(80, 255, 0, 0), // đỏ nhạt
          offset: Offset(-6, 0), // bóng sang trái
          blurRadius: 40,
          spreadRadius: 1,
        ),
      ],
    ),
    alignment: Alignment.center,
    child: Iconbutton(rebuild: rebuild, door: items, client: client),
  );
}
