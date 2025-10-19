// Minimal stub for platforms without BLE (web). Provides same class names
// but with no-op implementations so app compiles.

class DiscoveredDevice {
  final String id;
  final String name;
  DiscoveredDevice(this.id, this.name);
}

class ConnectionStateUpdate {
  final String deviceId;
  final String connectionState;
  ConnectionStateUpdate(this.deviceId, this.connectionState);
}

class BluetoothManager {
  Stream<DiscoveredDevice> scan({String? nameFilter}) async* {
    // no devices on web
  }

  Future<void> connect(
      String deviceId, void Function(ConnectionStateUpdate) onUpdate) async {}
  Stream<List<int>> subscribeToCharacteristic(
      String deviceId, String serviceUuid, String characteristicUuid) async* {
    // no notifications on web
  }
  Future<void> writeToCharacteristic(String deviceId, String serviceUuid,
      String characteristicUuid, List<int> bytes,
      {bool withResponse = true}) async {}
  Future<void> disconnect(String deviceId) async {}
  Future<void> disconnectAll() async {}
  void dispose() {}
  Future<void> saveDeviceSlot(
      String slotKey, String deviceId, String name) async {}
  Future<Map<String, String>?> loadDeviceSlot(String slotKey) async => null;
}
