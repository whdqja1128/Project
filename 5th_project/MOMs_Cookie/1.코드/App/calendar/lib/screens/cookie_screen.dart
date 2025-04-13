import 'package:flutter/material.dart';
import '../widgets/turtle_bot_control_buttons.dart';

class TurtleControlScreen extends StatelessWidget {
  const TurtleControlScreen({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('쿠키 조작'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.pop(context),
        ),
      ),
      body: SingleChildScrollView(
        child: Padding(
          padding: const EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Card(
                elevation: 3,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: const [
                      Text(
                        '쿠키 로봇 제어',
                        style: TextStyle(
                          fontSize: 20,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 8),
                      Text(
                        '아래 버튼을 눌러 쿠키 로봇을 원하는 위치로 이동시킬 수 있습니다. 명령 전송에는 몇 초가 소요될 수 있습니다.',
                        style: TextStyle(fontSize: 14, color: Colors.grey),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 16),
              const TurtleBotControlButtons(),
              const SizedBox(height: 24),
              
              // 추가 정보 카드
              Card(
                elevation: 2,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: const [
                      Text(
                        '위치 설명',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 12),

                      LocationInfoItem(
                        icon: Icons.water_drop,
                        title: '부엌',
                        description: '로봇이 물 공간으로 이동합니다.',
                      ),
                      Divider(),
                      LocationInfoItem(
                        icon: Icons.bathtub,
                        title: '욕실',
                        description: '로봇이 욕실로 이동합니다.',
                      ),
                      Divider(),
                      LocationInfoItem(
                        icon: Icons.weekend,
                        title: '거실',
                        description: '로봇이 거실로 이동합니다.',
                      ),
                      Divider(),
                      LocationInfoItem(
                        icon: Icons.bed,
                        title: '침실',
                        description: '로봇이 침실로 이동합니다.',
                      ),

                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class LocationInfoItem extends StatelessWidget {
  final IconData icon;
  final String title;
  final String description;

  const LocationInfoItem({
    Key? key,
    required this.icon,
    required this.title,
    required this.description,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8.0),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: Colors.blue, size: 24),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 4),
                Text(
                  description,
                  style: const TextStyle(fontSize: 14),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}