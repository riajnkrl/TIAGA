import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';
// tflite_flutter uses dart:ffi which is not available on web. Use a
// conditional import so building for web picks up a small stub that
// provides the minimal API used by this app.
import 'tflite_stub.dart'
    if (dart.library.io) 'package:tflite_flutter/tflite_flutter.dart';
// import 'package:image/image.dart' as img; // keep commented for future preprocessing use
import 'package:flutter_svg/flutter_svg.dart';
import 'services/notification_service.dart';
import 'package:permission_handler/permission_handler.dart';
// import 'services/bluetooth_service.dart';

// loading screen removed - direct to home

void main() {
  runApp(const PlantBotApp());
}

// Global messenger key so we can show SnackBars without relying on a
// BuildContext after async gaps (avoids analyzer warnings).
final GlobalKey<ScaffoldMessengerState> messengerKey =
    GlobalKey<ScaffoldMessengerState>();

class PlantBotApp extends StatelessWidget {
  const PlantBotApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'TIAGA',
      debugShowCheckedModeBanner: false,
      scaffoldMessengerKey: messengerKey,
      theme: ThemeData(
        primarySwatch: Colors.green,
        scaffoldBackgroundColor: const Color(0xFFF0FDF4),
        fontFamily: 'Poppins',
      ),
      home: const PlantBotHomePage(),
    );
  }
}

class PlantBotHomePage extends StatefulWidget {
  const PlantBotHomePage({super.key});

  @override
  State<PlantBotHomePage> createState() => _PlantBotHomePageState();
}

class _PlantBotHomePageState extends State<PlantBotHomePage>
    with SingleTickerProviderStateMixin {
  // ESP32 Configuration
  // Robot (control/sensor) - robot will run as AP at 192.168.4.1
  static const String esp32Ip = '192.168.4.1';
  static const String esp32Port = '80';
  // Camera stream
  static const String cameraIp = '192.168.4.2';
  static const String cameraPort = '81';

  // Plant status
  double plantHealth = 100.0;
  double soilMoisture = 50.0;
  // history of soil moisture and health samples
  final List<Map<String, dynamic>> _history = [];
  String tomatoMood = 'happy';
  String detectedDisease = 'Healthy';
  String tomatoMessage = "I'm feeling great! 🌱";
  String actionFeedback = '';
  // Advanced control state
  int _speedLevel = 2; // 1..3
  double _servoAngle = 90.0; // 0..180
  bool _camLedEnabled = false;
  Timer? _feedbackTimer;
  Timer? _sensorTimer;
  Timer? _healthCheckTimer;

  // TensorFlow Lite
  Interpreter? _interpreter;

  // WiFi manager or other communication manager can be added here

  // Animation
  late AnimationController _bounceController;
  late Animation<double> _bounceAnimation;

  // Disease messages mapping
  final Map<String, Map<String, dynamic>> diseaseInfo = {
    'Healthy': {
      'message': "I'm feeling great! Keep up the good care! 🌱",
      'mood': 'happy',
      'health': 100.0
    },
    'plant-health': {
      'message': "I'm doing well! Thanks for taking care of me! 😊",
      'mood': 'happy',
      'health': 90.0
    },
    'Bacterial Spot': {
      'message': "I have bacterial spots... I need help! 😰",
      'mood': 'sad',
      'health': 40.0
    },
    'Early Blight': {
      'message': "Early blight detected... Please treat me! 😟",
      'mood': 'sad',
      'health': 45.0
    },
    'Late Blight': {
      'message': "Late blight is serious! I need urgent care! 😢",
      'mood': 'sad',
      'health': 30.0
    },
    'Leaf Mold': {
      'message': "My leaves have mold... Help me breathe! 😔",
      'mood': 'sad',
      'health': 50.0
    },
    'Leaf_Miner': {
      'message': "Leaf miners are bothering me! 😣",
      'mood': 'sad',
      'health': 55.0
    },
    'Mosaic Virus': {
      'message': "I have mosaic virus... I'm not feeling well! 😞",
      'mood': 'sad',
      'health': 35.0
    },
    'Septoria': {
      'message': "Septoria leaf spot is here... I need treatment! 😟",
      'mood': 'sad',
      'health': 42.0
    },
    'Spider Mites': {
      'message': "Spider mites are attacking me! Help! 😰",
      'mood': 'sad',
      'health': 48.0
    },
    'Yellow Leaf Curl Virus': {
      'message': "Yellow curl virus... I'm struggling! 😢",
      'mood': 'sad',
      'health': 32.0
    },
  };

  @override
  void initState() {
    super.initState();
    _bounceController = AnimationController(
      duration: const Duration(milliseconds: 1000),
      vsync: this,
    )..repeat(reverse: true);
    _bounceAnimation = Tween<double>(begin: 0, end: -10).animate(
      CurvedAnimation(parent: _bounceController, curve: Curves.easeInOut),
    );
    _updateMood();
    _startSensorPolling();
    _startHealthChecking();
    _loadModel();
    // Notification initialization deferred until user allows notifications
    // No Bluetooth permissions needed for WiFi
  }

  // No Bluetooth permission logic needed

  // Notification permission state: null = unknown, otherwise a PermissionStatus
  PermissionStatus? _notificationPermissionStatus;

  // Notification initialization is performed lazily when the user opts in.

  @override
  void dispose() {
    _bounceController.dispose();
    _feedbackTimer?.cancel();
    _sensorTimer?.cancel();
    _healthCheckTimer?.cancel();
    _interpreter?.close();
    super.dispose();
  }

  Future<void> _loadModel() async {
    try {
      // Load your TFLite model here
      // _interpreter = await Interpreter.fromAsset('assets/tomato_model.tflite');
      debugPrint('TensorFlow Lite model loaded');
    } catch (e) {
      debugPrint('Error loading model: $e');
    }
  }

  void _startSensorPolling() {
    _sensorTimer = Timer.periodic(const Duration(seconds: 3), (timer) {
      _fetchSoilMoisture();
    });
  }

  void _startHealthChecking() {
    // Check plant health every 10 seconds using the camera stream
    _healthCheckTimer = Timer.periodic(const Duration(seconds: 10), (timer) {
      _checkPlantHealth();
    });
  }

  // --- WiFi UI / helpers (to be implemented) ---

  Future<void> _fetchSoilMoisture() async {
    try {
      final url = Uri.parse('http://$esp32Ip:$esp32Port/sensor');
      final response = await http.get(url).timeout(const Duration(seconds: 2));
      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        setState(() {
          // Assuming ESP32 sends moisture percentage (0-100)
          soilMoisture = (data['moisture'] ?? 50.0).toDouble();
          // add a history entry when we successfully fetch moisture
          _addHistoryEntry();
        });
      }
    } catch (e) {
      debugPrint('Error fetching soil moisture: $e');
    }
  }

  void _addHistoryEntry() {
    // keep a modest cap on history length
    final entry = {
      'time': DateTime.now().toIso8601String(),
      'soil': soilMoisture,
      'health': plantHealth,
      'disease': detectedDisease,
    };
    setState(() {
      _history.insert(0, entry);
      if (_history.length > 100) _history.removeLast();
    });
  }

  Future<void> _checkPlantHealth() async {
    try {
      // This would capture a frame from the stream and run inference
      // For now, simulating disease detection
      // In production, you'd process the camera stream frame

      // Placeholder for actual TensorFlow Lite inference
      // final result = await _runInference(cameraFrame);

      // For demonstration, randomly simulate detection
      // Replace this with actual model inference
      final diseases = diseaseInfo.keys.toList();
      final detected = diseases[DateTime.now().second % diseases.length];

      _updateDiseaseStatus(detected);
    } catch (e) {
      debugPrint('Error checking plant health: $e');
    }
  }

  void _updateDiseaseStatus(String disease) {
    if (diseaseInfo.containsKey(disease)) {
      setState(() {
        detectedDisease = disease;
        final info = diseaseInfo[disease]!;
        tomatoMessage = info['message'] as String;
        tomatoMood = info['mood'] as String;
        plantHealth = info['health'] as double;
      });
      _updateMood();
    }
  }

  void _updateMood() {
    setState(() {
      if (plantHealth > 75) {
        tomatoMood = 'happy';
      } else if (plantHealth > 50) {
        tomatoMood = 'neutral';
      } else {
        tomatoMood = 'sad';
      }
    });
  }

  // Legacy direct /Car?move= commands removed; using camera webserver /control endpoint.

  // New: send commands following the camera webserver API from Lesson 6-2
  Future<void> _sendCarDirection(String direction) async {
    await _sendControlCmd('car', {'direction': direction});
  }

  Future<void> _sendControlCmd(String cmd,
      [Map<String, String>? params]) async {
    try {
      final uri = Uri.parse('http://$esp32Ip:$esp32Port/control');
      final allParams = <String, String>{'cmd': cmd};
      if (params != null) allParams.addAll(params);
      final url = uri.replace(queryParameters: allParams);
      await http.get(url).timeout(const Duration(seconds: 2));
    } catch (e) {
      debugPrint('Error sending control cmd: $e');
      _showFeedback('⚠️ Connection failed');
    }
  }

  void _showFeedback(String message) {
    setState(() {
      actionFeedback = message;
    });
    _feedbackTimer?.cancel();
    _feedbackTimer = Timer(const Duration(seconds: 2), () {
      setState(() {
        actionFeedback = '';
      });
    });
  }

  void _handleMovement(String command, String label) {
    // command here is the direction string expected by the ESP32 camera code,
    // e.g. 'Forward', 'Backward', 'Left', 'Right', 'Clockwise', 'Anticlockwise', 'LeftUp', 'RightUp', etc.
    _sendCarDirection(command);
    _showFeedback('Moving $label...');
  }

  // legacy watering handler removed (replaced by soil check)

  Future<void> _handleSoilCheck() async {
    setState(() {
      actionFeedback = 'Checking soil...';
    });
    try {
      // Preferred: use compatibility GET to /control?cmd=soil_check (robot handles this)
      final controlUri = Uri.parse('http://$esp32Ip:$esp32Port/control')
          .replace(queryParameters: {'cmd': 'soil_check'});
      try {
        final response =
            await http.get(controlUri).timeout(const Duration(seconds: 2));
        if (response.statusCode == 200) {
          messengerKey.currentState?.showSnackBar(
              const SnackBar(content: Text('Requested soil check')));
          return;
        }
      } catch (e) {
        // fall through to POST fallback
        debugPrint('GET /control?cmd=soil_check failed: $e');
      }

      // Fallback: POST single-char 's' to /control (robot recognizes this)
      final postUri = Uri.parse('http://$esp32Ip:$esp32Port/control');
      try {
        final resp = await http
            .post(postUri, body: 's')
            .timeout(const Duration(seconds: 2));
        if (resp.statusCode == 200) {
          messengerKey.currentState?.showSnackBar(
              const SnackBar(content: Text('Requested soil check (POST)')));
        } else {
          messengerKey.currentState?.showSnackBar(
              const SnackBar(content: Text('Failed to send soil check')));
        }
      } catch (e) {
        messengerKey.currentState?.showSnackBar(
            SnackBar(content: Text('Failed to send soil check: $e')));
      }
    } catch (e) {
      messengerKey.currentState?.showSnackBar(
          SnackBar(content: Text('Failed to send soil check: $e')));
    } finally {
      Future.delayed(const Duration(seconds: 6), () {
        if (mounted) {
          setState(() {
            actionFeedback = '';
          });
        }
      });
    }
  }

  // (Connect SmartCar UI removed) quick reachability check helper removed

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              Color(0xFFF0FDF4),
              Color(0xFFD1FAE5),
              Color(0xFFCCFBF1),
            ],
          ),
        ),
        child: SafeArea(
          child: SingleChildScrollView(
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: LayoutBuilder(
                builder: (context, constraints) {
                  if (constraints.maxWidth > 800) {
                    return Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(child: _buildLeftPanel()),
                        const SizedBox(width: 20),
                        Expanded(child: _buildRightPanel()),
                      ],
                    );
                  } else {
                    return Column(
                      children: [
                        _buildLeftPanel(),
                        const SizedBox(height: 20),
                        _buildRightPanel(),
                      ],
                    );
                  }
                },
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Column(
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // App logo (SVG) - fall back to icon if asset fails
            SizedBox(
              width: 36,
              height: 36,
              child: SvgPicture.asset(
                'assets/images/tomato_logo.svg',
                semanticsLabel: 'Tomato logo',
                placeholderBuilder: (context) =>
                    Icon(Icons.eco, color: Colors.green[700], size: 32),
              ),
            ),
            const SizedBox(width: 8),
            IconButton(
              tooltip: 'Connections',
              onPressed: () {
                // Connections panel removed; show network info instead
                messengerKey.currentState?.showSnackBar(const SnackBar(
                    content: Text('Use WiFi to connect to ESP32')));
              },
              icon: const Icon(Icons.wifi),
            ),
            const SizedBox(width: 8),
            Text(
              'TIAGA',
              style: TextStyle(
                fontSize: 32,
                fontWeight: FontWeight.bold,
                color: Colors.green[900],
              ),
            ),
            const SizedBox(width: 8),
            SizedBox(
              width: 36,
              height: 36,
              child: SvgPicture.asset(
                'assets/images/tomato_logo.svg',
                semanticsLabel: 'Tomato logo',
                placeholderBuilder: (context) =>
                    Icon(Icons.eco, color: Colors.green[700], size: 32),
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        Text(
          'AI-Powered Tomato Plant Monitoring',
          style: TextStyle(
            fontSize: 14,
            color: Colors.green[600],
          ),
        ),
      ],
    );
  }

  Widget _buildNotificationPrompt() {
    final permanentlyDenied =
        _notificationPermissionStatus == PermissionStatus.permanentlyDenied;

    return Card(
      color: Colors.yellow[50],
      margin: const EdgeInsets.symmetric(vertical: 8),
      child: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Row(
          children: [
            const Icon(Icons.notifications_active_outlined,
                color: Colors.orange),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Enable notifications',
                    style: TextStyle(
                      fontWeight: FontWeight.bold,
                      color: Colors.orange[800],
                    ),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    permanentlyDenied
                        ? 'Notifications are disabled. Open settings to allow notifications.'
                        : 'We use notifications to remind you to water and maintain your plant.',
                    style: TextStyle(color: Colors.orange[700]),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 12),
            if (permanentlyDenied)
              ElevatedButton(
                onPressed: _openNotificationSettings,
                child: const Text('Open settings'),
              )
            else
              ElevatedButton(
                onPressed: _requestNotificationPermission,
                child: const Text('Allow'),
              )
          ],
        ),
      ),
    );
  }

  Future<void> _requestNotificationPermission() async {
    final status = await NotificationService.instance.requestPermissions();
    if (mounted) {
      setState(() {
        _notificationPermissionStatus = status;
      });
    }
    if (status.isGranted) {
      // Schedule reminders now that permission was granted
      await NotificationService.instance.scheduleDailyWatering(
        id: 1,
        title: 'Water your plant',
        body: 'It\'s time to water your plant for healthy growth!',
        time: const TimeOfDay(hour: 9, minute: 0),
      );
    }
  }

  Future<void> _openNotificationSettings() async {
    await NotificationService.instance.openSettings();
  }

  Widget _buildLeftPanel() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _buildHeader(),
        const SizedBox(height: 12),
        if (_notificationPermissionStatus == null ||
            !_notificationPermissionStatus!.isGranted)
          _buildNotificationPrompt(),
        const SizedBox(height: 12),
        _buildCharacterCard(),
        const SizedBox(height: 16),
        _buildCameraCard(),
      ],
    );
  }

  Widget _buildCharacterCard() {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(24),
        boxShadow: [
          BoxShadow(
            color: Colors.green.withAlpha((0.2 * 255).round()),
            blurRadius: 20,
            offset: const Offset(0, 10),
          ),
        ],
        border: Border.all(color: Colors.green[200]!, width: 4),
      ),
      padding: const EdgeInsets.all(24),
      child: Column(
        children: [
          _buildTomatoCharacter(),
          const SizedBox(height: 16),
          _buildSpeechBubble(),
          const SizedBox(height: 16),
          Text(
            'Tommy Tomato',
            style: TextStyle(
              fontSize: 24,
              fontWeight: FontWeight.bold,
              color: Colors.green[900],
            ),
          ),
          Text(
            'Status: $detectedDisease',
            style: TextStyle(
              fontSize: 14,
              color: detectedDisease == 'Healthy' ||
                      detectedDisease == 'plant-health'
                  ? Colors.green[600]
                  : Colors.red[600],
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 24),
          _buildHealthBars(),
          const SizedBox(height: 24),
          _buildCareActions(),
        ],
      ),
    );
  }

  Widget _buildSpeechBubble() {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.green[50],
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: Colors.green[200]!, width: 2),
      ),
      child: Row(
        children: [
          const Text(
            '💬',
            style: TextStyle(fontSize: 20),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              tomatoMessage,
              style: TextStyle(
                fontSize: 13,
                color: Colors.green[900],
                fontWeight: FontWeight.w500,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildTomatoCharacter() {
    Map<String, dynamic> faceData = {
      'happy': {'eyes': '◕ ◕', 'mouth': '‿', 'color': const Color(0xFFFF6B6B)},
      'neutral': {
        'eyes': '• •',
        'mouth': '—',
        'color': const Color(0xFFFF8787)
      },
      'sad': {'eyes': '╥ ╥', 'mouth': '︵', 'color': const Color(0xFFFFA07A)},
    };

    final face = faceData[tomatoMood]!;

    return AnimatedBuilder(
      animation: _bounceAnimation,
      builder: (context, child) {
        return Transform.translate(
          offset: tomatoMood == 'happy'
              ? Offset(0, _bounceAnimation.value)
              : Offset.zero,
          child: child,
        );
      },
      child: Stack(
        clipBehavior: Clip.none,
        children: [
          Container(
            width: 120,
            height: 120,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              gradient: RadialGradient(
                center: const Alignment(-0.3, -0.3),
                colors: [
                  (face['color'] as Color).withAlpha((0.8 * 255).round()),
                  face['color'] as Color,
                ],
              ),
              boxShadow: tomatoMood == 'happy'
                  ? [
                      BoxShadow(
                        color: Colors.red.withAlpha((0.3 * 255).round()),
                        blurRadius: 20,
                        spreadRadius: 5,
                      )
                    ]
                  : [],
            ),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const SizedBox(height: 10),
                Text(
                  face['eyes'] as String,
                  style: const TextStyle(
                    fontSize: 28,
                    color: Colors.white,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  face['mouth'] as String,
                  style: const TextStyle(
                    fontSize: 32,
                    color: Colors.white,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
          ),
          Positioned(
            top: -10,
            left: 50,
            child: CustomPaint(
              size: const Size(20, 20),
              painter: StemPainter(),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildHealthBars() {
    return Column(
      children: [
        _buildStatusBar(
          'Health',
          plantHealth,
          Icons.eco,
          Colors.green,
        ),
        const SizedBox(height: 12),
        _buildStatusBar(
          'Soil Moisture',
          soilMoisture,
          Icons.water_drop,
          Colors.blue,
        ),
      ],
    );
  }

  Widget _buildStatusBar(
      String label, double value, IconData icon, MaterialColor color) {
    return Column(
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Icon(icon, size: 16, color: color[700]),
                const SizedBox(width: 4),
                Text(
                  label,
                  style: TextStyle(
                    fontSize: 14,
                    fontWeight: FontWeight.w600,
                    color: color[700],
                  ),
                ),
              ],
            ),
            Text(
              '${value.toInt()}%',
              style: TextStyle(
                fontSize: 14,
                color: color[600],
              ),
            ),
          ],
        ),
        const SizedBox(height: 6),
        Container(
          height: 12,
          decoration: BoxDecoration(
            color: color[100],
            borderRadius: BorderRadius.circular(6),
          ),
          child: FractionallySizedBox(
            alignment: Alignment.centerLeft,
            widthFactor: value / 100,
            child: Container(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: [color[400]!, color[600]!],
                ),
                borderRadius: BorderRadius.circular(6),
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildCareActions() {
    return SizedBox(
      width: double.infinity,
      child: Row(
        children: [
          Expanded(
            child: ElevatedButton.icon(
              onPressed: _handleSoilCheck,
              icon: const Icon(Icons.grass, size: 20),
              label: const Text('Check Soil Moisture'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.blue[400],
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 16),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
              ),
            ),
          ),
          const SizedBox(width: 12),
          ElevatedButton.icon(
            onPressed: () async {
              // send a retract command to the ESP32 cam servo
              await _sendRetractCommand();
            },
            icon: const Icon(Icons.reply, size: 18),
            label: const Text('Retract Probe'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.grey[200],
              foregroundColor: Colors.black,
              padding: const EdgeInsets.symmetric(vertical: 14, horizontal: 12),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _sendRetractCommand() async {
    // The ESP32 camera/servo should accept a specific control command to retract
    // We'll send a 'servo' command with angle=0 (assumption) and a short feedback
    try {
      await _sendControlCmd('servo', {'angle': '0'});
      _showFeedback('Retracting probe');
      // Optionally add a small delay and then record state
      Future.delayed(const Duration(seconds: 1), () {
        _addHistoryEntry();
      });
    } catch (e) {
      _showFeedback('Retract failed');
    }
  }

  Widget _buildCameraCard() {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(24),
        boxShadow: [
          BoxShadow(
            color: Colors.green.withAlpha((0.2 * 255).round()),
            blurRadius: 20,
            offset: const Offset(0, 10),
          ),
        ],
        border: Border.all(color: Colors.green[200]!, width: 4),
      ),
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          Text(
            'Live Plant Monitoring',
            style: TextStyle(
              fontSize: 18,
              fontWeight: FontWeight.bold,
              color: Colors.green[900],
            ),
          ),
          const SizedBox(height: 12),
          AspectRatio(
            aspectRatio: 16 / 9,
            child: Container(
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                  colors: [Color(0xFF1F2937), Color(0xFF111827)],
                ),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Image.network(
                'http://$cameraIp:$cameraPort/stream',
                fit: BoxFit.cover,
                errorBuilder: (context, error, stackTrace) {
                  return const Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(
                          Icons.videocam_off,
                          color: Colors.white54,
                          size: 48,
                        ),
                        SizedBox(height: 8),
                        Text(
                          'Camera connecting...',
                          style: TextStyle(color: Colors.white),
                        ),
                      ],
                    ),
                  );
                },
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildRightPanel() {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(24),
        boxShadow: [
          BoxShadow(
            color: Colors.green.withAlpha((0.2 * 255).round()),
            blurRadius: 20,
            offset: const Offset(0, 10),
          ),
        ],
        border: Border.all(color: Colors.green[200]!, width: 4),
      ),
      padding: const EdgeInsets.all(24),
      child: Column(
        children: [
          Text(
            '🤖 Robot Controls',
            style: TextStyle(
              fontSize: 22,
              fontWeight: FontWeight.bold,
              color: Colors.green[900],
            ),
          ),
          const SizedBox(height: 16),
          if (actionFeedback.isNotEmpty)
            Container(
              padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
              margin: const EdgeInsets.only(bottom: 16),
              decoration: BoxDecoration(
                color: Colors.green[100],
                borderRadius: BorderRadius.circular(12),
              ),
              child: Text(
                actionFeedback,
                style: TextStyle(
                  color: Colors.green[800],
                  fontWeight: FontWeight.w600,
                ),
                textAlign: TextAlign.center,
              ),
            ),
          _buildControlPad(),
          const SizedBox(height: 16),
          _buildAdvancedControls(),
          const SizedBox(height: 24),
          // Compact history panel
          _buildHistoryPanel(),
        ],
      ),
    );
  }

  // Telemetry panel removed per request

  Widget _buildAdvancedControls() {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.green[50]!, width: 1),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            'Advanced Controls',
            style: TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.bold,
              color: Colors.green[800],
            ),
          ),
          const SizedBox(height: 12),
          // Speed controls (preset buttons + slider)
          Row(
            children: [
              Expanded(
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    for (var i = 1; i <= 3; i++)
                      ElevatedButton(
                        style: ElevatedButton.styleFrom(
                          backgroundColor: _speedLevel == i
                              ? Colors.green
                              : Colors.grey[300],
                          foregroundColor:
                              _speedLevel == i ? Colors.white : Colors.black,
                        ),
                        onPressed: () async {
                          setState(() => _speedLevel = i);
                          await _sendControlCmd(
                              'speed', {'value': '$_speedLevel'});
                          _showFeedback('Speed set to $_speedLevel');
                        },
                        child: Text('S$i'),
                      ),
                  ],
                ),
              ),
            ],
          ),
          Slider(
            value: _speedLevel.toDouble(),
            min: 1,
            max: 3,
            divisions: 2,
            label: 'Speed $_speedLevel',
            onChanged: (v) async {
              setState(() => _speedLevel = v.round());
              await _sendControlCmd('speed', {'value': '$_speedLevel'});
            },
          ),
          const SizedBox(height: 8),
          // Servo control
          Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Servo Angle: ${_servoAngle.toInt()}°',
                        style: TextStyle(color: Colors.green[700])),
                    Slider(
                      value: _servoAngle,
                      min: 0,
                      max: 180,
                      divisions: 18,
                      label: '${_servoAngle.toInt()}°',
                      onChanged: (v) {
                        setState(() => _servoAngle = v);
                      },
                      onChangeEnd: (v) async {
                        await _sendControlCmd(
                            'servo', {'angle': '${v.toInt()}'});
                        _showFeedback('Servo ${v.toInt()}°');
                      },
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          // CAM LED toggle
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text('CAM LED', style: TextStyle(color: Colors.green[800])),
              Switch(
                value: _camLedEnabled,
                onChanged: (v) async {
                  setState(() => _camLedEnabled = v);
                  await _sendControlCmd('CAM_LED', {'value': v ? '1' : '0'});
                  _showFeedback(v ? 'CAM LED on' : 'CAM LED off');
                },
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildControlPad() {
    return LayoutBuilder(builder: (context, constraints) {
      // Compute a responsive button size based on available width so the control
      // pad can fit on narrow screens without overflowing.
      final maxWidth = constraints.maxWidth.isFinite && constraints.maxWidth > 0
          ? constraints.maxWidth
          : MediaQuery.of(context).size.width;
      // Reserve padding and spacing (three buttons + two spacers ~= 2*16)
      final available = (maxWidth - 2 * 20).clamp(160.0, maxWidth);
      // Target up to 3 buttons across; clamp to reasonable min/max sizes
      double btnSize = (available - 2 * 16) / 3;
      btnSize = btnSize.clamp(56.0, 80.0);

      return Container(
        padding: const EdgeInsets.all(20),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Colors.green[50]!, Colors.teal[50]!],
          ),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: Colors.green[200]!, width: 2),
        ),
        child: Column(
          children: [
            // Top row (map to camera server directions)
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _buildDirectionButton('↺', 'Anticlockwise', Colors.teal,
                    size: btnSize),
                const SizedBox(width: 16),
                _buildDirectionButton('↑', 'Forward', Colors.green,
                    size: btnSize),
                const SizedBox(width: 16),
                _buildDirectionButton('↻', 'Clockwise', Colors.teal,
                    size: btnSize),
              ],
            ),
            const SizedBox(height: 16),
            // Middle row
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                _buildDirectionButton('←', 'Left', Colors.teal, size: btnSize),
                const SizedBox(width: 16),
                _buildStopButton(size: btnSize),
                const SizedBox(width: 16),
                _buildDirectionButton('→', 'Right', Colors.teal, size: btnSize),
              ],
            ),
            const SizedBox(height: 16),
            // Bottom row
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                SizedBox(width: btnSize),
                const SizedBox(width: 16),
                _buildDirectionButton('↓', 'Backward', Colors.green,
                    size: btnSize),
                const SizedBox(width: 16),
                SizedBox(width: btnSize),
              ],
            ),
          ],
        ),
      );
    });
  }

  Widget _buildDirectionButton(String icon, String command, MaterialColor color,
      {double size = 80}) {
    return GestureDetector(
      onTapDown: (_) => _handleMovement(command, icon),
      onTapUp: (_) => _sendCarDirection('stop'),
      onTapCancel: () => _sendCarDirection('stop'),
      child: Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [color[300]!, color[600]!],
          ),
          borderRadius: BorderRadius.circular(16),
          boxShadow: [
            BoxShadow(
              color: color.withAlpha((0.4 * 255).round()),
              blurRadius: 8,
              offset: const Offset(0, 4),
            ),
          ],
        ),
        child: Center(
          child: Text(
            icon,
            style: TextStyle(
              fontSize: (size * 0.45).clamp(18.0, 36.0),
              color: Colors.white,
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildStopButton({double size = 80}) {
    return GestureDetector(
      onTap: () {
        _sendCarDirection('stop');
        _showFeedback('Stopping');
      },
      child: Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Colors.red[400]!, Colors.red[700]!],
          ),
          borderRadius: BorderRadius.circular(16),
          boxShadow: [
            BoxShadow(
              color: Colors.red.withAlpha((0.4 * 255).round()),
              blurRadius: 8,
              offset: const Offset(0, 4),
            ),
          ],
        ),
        child: Center(
          child: Text(
            '⬛',
            style: TextStyle(
              fontSize: (size * 0.4).clamp(14.0, 32.0),
              color: Colors.white,
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
      ),
    );
  }

  // Connection info removed per request

  Widget _buildHistoryPanel() {
    final recent = _history.take(5).toList();
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Colors.green[50]!, width: 1),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text('History',
                  style: TextStyle(
                      fontWeight: FontWeight.bold, color: Colors.green[800])),
              TextButton(
                onPressed: _history.isEmpty ? null : _showHistoryDialog,
                child: const Text('View All'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          if (recent.isEmpty)
            SizedBox(
              height: 80,
              child: Center(
                child: Text('No history yet',
                    style: TextStyle(color: Colors.green[300])),
              ),
            )
          else
            Column(
              children: recent.map((e) {
                final t = DateTime.parse(e['time']).toLocal();
                final timeStr =
                    '${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}';
                return ListTile(
                  dense: true,
                  contentPadding: EdgeInsets.zero,
                  title: Text('$timeStr — ${e['disease']}',
                      style: const TextStyle(fontSize: 13)),
                  subtitle: Text(
                      'Soil: ${e['soil'].toStringAsFixed(0)}% · Health: ${e['health'].toStringAsFixed(0)}%',
                      style: TextStyle(fontSize: 12, color: Colors.grey[700])),
                );
              }).toList(),
            ),
        ],
      ),
    );
  }

  void _showHistoryDialog() {
    showDialog(
      context: context,
      builder: (context) {
        return AlertDialog(
          title: const Text('Full History'),
          content: SizedBox(
            width: double.maxFinite,
            height: 400,
            child: _history.isEmpty
                ? const Center(child: Text('No history recorded'))
                : ListView.separated(
                    itemCount: _history.length,
                    separatorBuilder: (_, __) => const Divider(height: 1),
                    itemBuilder: (context, idx) {
                      final e = _history[idx];
                      final t = DateTime.parse(e['time']).toLocal();
                      final ts =
                          '${t.year}-${t.month.toString().padLeft(2, '0')}-${t.day.toString().padLeft(2, '0')} ${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}:${t.second.toString().padLeft(2, '0')}';
                      return ListTile(
                        title: Text(ts, style: const TextStyle(fontSize: 12)),
                        subtitle: Text(
                            '${e['disease']} · Soil ${e['soil'].toStringAsFixed(0)}% · Health ${e['health'].toStringAsFixed(0)}%'),
                      );
                    },
                  ),
          ),
          actions: [
            TextButton(
                onPressed: () => Navigator.of(context).pop(),
                child: const Text('Close')),
          ],
        );
      },
    );
  }
}

class StemPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = Colors.green[700]!
      ..style = PaintingStyle.fill;

    final stemPath = Path()
      ..moveTo(size.width / 2, size.height)
      ..lineTo(size.width / 2 - 4, 0)
      ..lineTo(size.width / 2 + 4, 0)
      ..close();
    canvas.drawPath(stemPath, paint);

    final leafPaint = Paint()
      ..color = Colors.green[600]!
      ..style = PaintingStyle.fill;

    canvas.drawOval(
      Rect.fromLTWH(size.width / 2 - 10, 5, 8, 12),
      leafPaint,
    );
    canvas.drawOval(
      Rect.fromLTWH(size.width / 2 + 2, 5, 8, 12),
      leafPaint,
    );
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
