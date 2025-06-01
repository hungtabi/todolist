import 'package:mqtt_client/mqtt_client.dart';

class Door_items {
  bool _door_state;
  String _password;

  Door_items({password = '123456', door_state = false})
    : _door_state = door_state,
      _password = password;
  String Door_password_get() => _password;

  set Door_password_set(String pass) => _password = pass;

  bool Door_state_get() => _door_state;

  set Door_state_set(bool state) => _door_state = state;
}
