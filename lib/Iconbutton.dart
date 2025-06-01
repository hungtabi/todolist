import 'package:flutter/material.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:todolist/Door_control.dart';
import 'package:todolist/data/Door_items.dart';
import 'package:todolist/Notificate.dart';
import 'package:todolist/dialog.dart';
import 'package:todolist/mqtt.dart';

class Iconbutton extends StatelessWidget {
  Iconbutton({
    super.key,
    required this.rebuild,
    required this.door,
    required this.client,
  });
  final Door_items door;
  final Function rebuild;
  final MqttServerClient? client;

  final TextEditingController controller_text = TextEditingController();
  void handleOnclick(BuildContext context) {
    final pass = controller_text.text;
    if (pass.isEmpty) {
      notificate(context, 'password is not empty \n try again');
    } else if (!(pass.length == 6)) {
      notificate(context, 'password has 6 number \n try again');
    } else {
      if (pass == door.Door_password_get()) {
        notificate(context, 'password is correct\n open the door');
        if (client != null) {
          publishPassword(client!, "password", pass);
        } else {
          print("MQTT client is null!");
        }
        rebuild();
      } else {
        notificate(context, 'password is not correct');
      }
    }
    Navigator.pop(context);
    controller_text.clear();
  }

  @override
  Widget build(BuildContext context) {
    return IconButton(
      onPressed: () {
        if (!door.Door_state_get()) {
          show_dialog(
            context,
            handleOnclick,
            controller_text,
            'unlock',
            'enter password',
          );
        } else {
          if (client != null) {
            publishPassword(client!, "password", "123");
          } else {
            print("MQTT client is null!");
          }
        }
      },
      icon: Image.asset(
        door.Door_state_get() ? 'assets/unlock.png' : 'assets/lock.png',
      ),
    );
  }
}
