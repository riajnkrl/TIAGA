import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:flutter/material.dart';
import 'package:timezone/timezone.dart' as tz;
import 'package:timezone/data/latest_all.dart' as tzdata;
import 'package:permission_handler/permission_handler.dart';

class NotificationService {
  NotificationService._privateConstructor();
  static final NotificationService _instance =
      NotificationService._privateConstructor();
  static NotificationService get instance => _instance;

  final FlutterLocalNotificationsPlugin _flutterLocalNotificationsPlugin =
      FlutterLocalNotificationsPlugin();

  Future<void> init() async {
    tzdata.initializeTimeZones();
    // Try to set a sensible local timezone. Avoid using platform timezone
    // plugin to keep Android build simpler; fall back to UTC if resolution fails.
    try {
      final localName = DateTime.now().timeZoneName;
      tz.setLocalLocation(tz.getLocation(localName));
    } catch (e) {
      tz.setLocalLocation(tz.UTC);
    }

    const AndroidInitializationSettings androidInitializationSettings =
        AndroidInitializationSettings('@mipmap/ic_launcher');

    const DarwinInitializationSettings iosInitializationSettings =
        DarwinInitializationSettings(
      requestAlertPermission: false,
      requestBadgePermission: false,
      requestSoundPermission: false,
    );

    const InitializationSettings initSettings = InitializationSettings(
      android: androidInitializationSettings,
      iOS: iosInitializationSettings,
    );

    await _flutterLocalNotificationsPlugin.initialize(initSettings);
  }

  /// Requests notification permission and returns the underlying [PermissionStatus].
  Future<PermissionStatus> requestPermissions() async {
    try {
      debugPrint(
          'NotificationService: Checking notification permission status');
      final status = await Permission.notification.status;

      if (status.isGranted) {
        debugPrint('NotificationService: Permission already granted');
        return status;
      }

      debugPrint('NotificationService: Requesting notification permission');
      final result = await Permission.notification.request();
      debugPrint('NotificationService: permission result: $result');
      return result;
    } catch (e, st) {
      debugPrint('NotificationService: Error requesting permissions: $e\n$st');
      return PermissionStatus.denied;
    }
  }

  /// Opens the app settings so the user can enable permissions manually.
  Future<bool> openSettings() async {
    try {
      debugPrint('NotificationService: Opening app settings');
      return await openAppSettings();
    } catch (e) {
      debugPrint('NotificationService: Failed to open settings: $e');
      return false;
    }
  }

  Future<void> scheduleDailyWatering({
    required int id,
    required String title,
    required String body,
    required TimeOfDay time,
  }) async {
    final now = tz.TZDateTime.now(tz.local);
    final scheduled = tz.TZDateTime(
        tz.local, now.year, now.month, now.day, time.hour, time.minute);
    final when = scheduled.isBefore(now)
        ? scheduled.add(const Duration(days: 1))
        : scheduled;

    await _flutterLocalNotificationsPlugin.zonedSchedule(
      id,
      title,
      body,
      when,
      const NotificationDetails(
        android: AndroidNotificationDetails(
          'plantbot_channel',
          'PlantBot Reminders',
          channelDescription: 'Reminders for watering and maintenance',
          importance: Importance.max,
          priority: Priority.high,
        ),
        iOS: DarwinNotificationDetails(),
      ),
      androidScheduleMode: AndroidScheduleMode.exactAllowWhileIdle,
      matchDateTimeComponents: DateTimeComponents.time,
    );
  }

  Future<void> scheduleWeeklyWeedReminder({
    required int id,
    required String title,
    required String body,
    required Day day,
    required TimeOfDay time,
  }) async {
    final now = tz.TZDateTime.now(tz.local);

    // Find next instance of the requested weekday at given time
    tz.TZDateTime scheduled = tz.TZDateTime(
        tz.local, now.year, now.month, now.day, time.hour, time.minute);
    while (scheduled.weekday != (day.index + 1)) {
      scheduled = scheduled.add(const Duration(days: 1));
    }
    if (scheduled.isBefore(now)) {
      scheduled = scheduled.add(const Duration(days: 7));
    }

    await _flutterLocalNotificationsPlugin.zonedSchedule(
      id,
      title,
      body,
      scheduled,
      const NotificationDetails(
        android: AndroidNotificationDetails(
          'plantbot_channel',
          'PlantBot Reminders',
          channelDescription: 'Reminders for watering and maintenance',
          importance: Importance.high,
          priority: Priority.high,
        ),
        iOS: DarwinNotificationDetails(),
      ),
      androidScheduleMode: AndroidScheduleMode.exactAllowWhileIdle,
      matchDateTimeComponents: DateTimeComponents.dayOfWeekAndTime,
    );
  }

  Future<void> cancel(int id) async {
    await _flutterLocalNotificationsPlugin.cancel(id);
  }

  Future<void> cancelAll() async {
    await _flutterLocalNotificationsPlugin.cancelAll();
  }
}
