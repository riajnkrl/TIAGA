// This is a basic Flutter widget test.
//
// To perform an interaction with a widget in your test, use the WidgetTester
// utility in the flutter_test package. For example, you can send tap and scroll
// gestures. You can also use WidgetTester to find child widgets in the widget
// tree, read text, and verify that the values of widget properties are correct.

import 'package:flutter_test/flutter_test.dart';

import 'package:plantbot_care/main.dart';

void main() {
  testWidgets('App loads and shows title', (WidgetTester tester) async {
    await tester.pumpWidget(const PlantBotApp());
    // Avoid pumpAndSettle since the app has repeating animations and network image
    // attempts; just advance a short duration so widgets build.
    await tester.pump(const Duration(milliseconds: 500));

    expect(find.text('TIAGA'), findsOneWidget);
    expect(find.text('Keep your plants happy and healthy!'), findsOneWidget);
  });
}
