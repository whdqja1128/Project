import 'package:flutter/material.dart';
import 'calendar_screen.dart';
import 'cookie_screen.dart';
import 'iot_screen.dart';
import 'alarm_screen.dart';

class MainScreen extends StatelessWidget {
  const MainScreen({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    final buttonWidth = size.width * 0.4; // 화면 너비의 40%
    final buttonHeight = 110.0; // 고정된 높이값

    return Scaffold(
      appBar: AppBar(title: const Text('MOMS Cookie'), centerTitle: true),
      body: Center(
        child: Column(
          children: [
            // 상단 여백 (화면 높이의 10%)
            SizedBox(height: size.height * 0.1),

            // 로고 텍스트
            const Text(
              'MOMS Cookie',
              style: TextStyle(
                fontSize: 36,
                fontWeight: FontWeight.bold,
                color: Colors.blue,
              ),
            ),

            // 여백
            const SizedBox(height: 40),

            // 버튼 컨테이너
            Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                children: [
                  // 첫 번째 줄 - 일정과 쿠키 조작
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      // 일정 버튼
                      _buildNavigationButton(
                        context,
                        '일정',
                        Icons.calendar_today,
                        Colors.blue,
                        () => Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => const CalendarScreen(),
                          ),
                        ),
                        width: buttonWidth,
                        height: buttonHeight,
                      ),

                      const SizedBox(width: 16),

                      // 쿠키 조작 버튼
                      _buildNavigationButton(
                        context,
                        '쿠키 조작',
                        Icons.smart_toy,
                        Colors.orange,
                        () => Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => const TurtleControlScreen(),
                          ),
                        ),
                        width: buttonWidth,
                        height: buttonHeight,
                      ),
                    ],
                  ),

                  // 두 번째 줄 - IoT 시스템과 알람 설정
                  const SizedBox(height: 16),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      // IoT 시스템 버튼
                      _buildNavigationButton(
                        context,
                        'IoT 시스템',
                        Icons.home_outlined,
                        Colors.green,
                        () => Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => const IoTControlScreen(),
                          ),
                        ),
                        width: buttonWidth,
                        height: buttonHeight,
                      ),

                      const SizedBox(width: 16),

                      // 알람 설정 버튼 (새로 추가)
                      _buildNavigationButton(
                        context,
                        '알람 설정',
                        Icons.alarm,
                        Colors.purple,
                        () => Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => const AlarmScreen(),
                          ),
                        ),
                        width: buttonWidth,
                        height: buttonHeight,
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildNavigationButton(
    BuildContext context,
    String label,
    IconData icon,
    Color color,
    VoidCallback onPressed, {
    required double width,
    required double height,
  }) {
    return SizedBox(
      width: width,
      height: height,
      child: ElevatedButton(
        style: ElevatedButton.styleFrom(
          foregroundColor: Colors.white,
          backgroundColor: color,
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 16),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),
        ),
        onPressed: onPressed,
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 36),
            const SizedBox(height: 8),
            Text(
              label,
              style: const TextStyle(fontSize: 16),
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}
