import 'package:flutter/material.dart';

ScaffoldFeatureController<SnackBar, SnackBarClosedReason> notificate(
  BuildContext context,
  String text,
) {
  return ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(
      backgroundColor: Color.fromARGB(255, 238, 6, 6),
      content: Text(text, textAlign: TextAlign.center),
      duration: Duration(seconds: 2),
    ),
  );
}
