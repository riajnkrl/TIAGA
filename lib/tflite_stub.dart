// Minimal stub for tflite_flutter so web builds don't fail.
// This provides the small subset of the API used by the app.

class Interpreter {
  Interpreter._();
  static Future<Interpreter> fromAsset(String asset) async => Interpreter._();
  void close() {}
}

// Export a type alias for convenience if code expects package types.
typedef TfLiteInterpreter = Interpreter;
