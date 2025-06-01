import 'package:flutter/material.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:todolist/Notificate.dart';
import 'package:todolist/data/Door_items.dart';
import 'package:todolist/dialog.dart';
import 'package:todolist/mqtt.dart';

TextButton text_button(
  BuildContext context,
  Door_items door,
  MqttServerClient? client,
) {
  final TextEditingController controllerText = TextEditingController();
  int state = 0;
  void handle(BuildContext context) {
    final pass = controllerText.text;
    if (state == 0) {
      if (pass == door.Door_password_get()) {
        notificate(context, 'password corret');
        state = 1;
        controllerText.clear();
        Navigator.pop(context);
        show_dialog(
          context,
          handle,
          controllerText,
          (state == 0) ? 'continues' : 'enter',
          (state == 0) ? 'enter old password' : 'enter new password',
        );
      } else {
        controllerText.clear();
        notificate(context, 'password incorret\n try again');
      }
    } else {
      if (pass.isEmpty) {
        notificate(context, 'password is not empty \n try again');
      } else if (!(pass.length == 6)) {
        notificate(context, 'password has 6 number \n try again');
      } else {
        (pass != door.Door_password_get())
            ? notificate(context, 'password was updated successfully')
            : notificate(context, 'new password same as old password');
        door.Door_password_set = pass;
        state = 0;
        controllerText.clear();
        Navigator.pop(context);
        if (client != null) {
          publishPassword(client, "change", pass);
        } else {
          print("MQTT client is null!");
        }
      }
    }
  }

  return TextButton(
    onPressed: () {
      state = 0;
      show_dialog(
        context,
        handle,
        controllerText,
        (state == 0) ? 'continues' : 'enter',
        (state == 0) ? 'enter old password' : 'enter new password',
      );
    },
    child: Container(
      alignment: Alignment.centerRight,
      width: double.infinity,
      child: Text(
        'Change password ?',
        style: TextStyle(color: Color.fromARGB(255, 238, 6, 6)),
      ),
    ),
  );
}
