import 'package:flutter/material.dart';
import 'package:todolist/data/Door_items.dart';

Container Door_state(Door_items door) {
  return Container(
    margin: EdgeInsets.symmetric(vertical: 20),
    alignment: Alignment.center,
    height: 50,
    width: double.infinity,
    color: Color.fromARGB(255, 238, 6, 6),
    child: Text(
      door.Door_state_get() ? 'DOOR OPEN' : 'DOOR CLOSE',
      style: TextStyle(
        fontWeight: FontWeight.bold,
        fontSize: 32,
        color: Color.fromARGB(255, 255, 255, 255),
      ),
    ),
  );
}
