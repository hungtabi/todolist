import 'package:flutter/material.dart';

Future<dynamic> show_dialog(
  BuildContext context,
  Function handle,
  TextEditingController controllerText,
  String text,
  String textHead,
) async {
  return showDialog(
    context: context,
    builder: (BuildContext context) {
      return AlertDialog(
        insetPadding: EdgeInsets.symmetric(horizontal: 10, vertical: 100),

        alignment: Alignment.center,
        title: Text(textHead, textAlign: TextAlign.center),
        actions: [
          Container(
            alignment: Alignment.bottomCenter,
            padding: EdgeInsets.symmetric(vertical: 20),
            child: Column(
              children: [
                TextField(
                  controller: controllerText,
                  decoration: InputDecoration(border: OutlineInputBorder()),
                ),
                const SizedBox(height: 30),
                SizedBox(
                  height: 30,
                  width: 200,
                  child: ElevatedButton(
                    onPressed: () => handle(context),
                    child: Text(text),
                  ),
                ),
              ],
            ),
          ),
        ],
      );
    },
  );
}
