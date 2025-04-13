import 'package:flutter/material.dart';
import '../services/socket_service.dart';

class TurtleBotControlButtons extends StatelessWidget {
  const TurtleBotControlButtons({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.all(8.0),
      elevation: 3,
      child: Padding(
        padding: const EdgeInsets.all(8.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Padding(
              padding: EdgeInsets.symmetric(horizontal: 8.0, vertical: 4.0),
              child: Text(
                '터틀봇 제어',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
            ),
            const Divider(),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _buildControlButton(
                  context,
                  '부엌',
                  Colors.lightBlue,
                  Icons.water_drop,
                  'WATERGO',
                ),
                _buildControlButton(
                  context,
                  '욕실',
                  Colors.teal,
                  Icons.bathtub,
                  'BATHGO',
                ),
                _buildControlButton(
                  context,
                  '거실',
                  Colors.green,
                  Icons.weekend,
                  'LIVINGGO',
                ),
                _buildControlButton(
                  context,
                  '침실',
                  Colors.purple,
                  Icons.bed,
                  'BEDGO',
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildControlButton(
    BuildContext context,
    String label,
    Color color,
    IconData icon,
    String command,
  ) {
    return ElevatedButton.icon(
      icon: Icon(icon, size: 20),
      label: Text(label),
      style: ElevatedButton.styleFrom(
        backgroundColor: color,
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      ),
      onPressed: () => _sendTurtleBotCommand(context, command),
    );
  }

  Future<void> _sendTurtleBotCommand(
    BuildContext context,
    String command,
  ) async {
    try {
      final socketService = SocketService();

      // 소켓 연결 확인
      if (!socketService.isConnected) {
        final connected = await socketService.connect();
        if (!connected) {
          if (context.mounted) {
            ScaffoldMessenger.of(context).showSnackBar(
              const SnackBar(
                content: Text('서버 연결에 실패했습니다. 설정을 확인해주세요.'),
                backgroundColor: Colors.red,
              ),
            );
          }
          return;
        }
      }

      // USR_LIN에게 명령 전송
      final sendResult = await socketService.sendMessage("USR_LIN:$command");

      if (sendResult) {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('터틀봇 명령이 전송되었습니다: $command'),
              backgroundColor: Colors.green,
              duration: const Duration(seconds: 2),
            ),
          );
        }
      } else {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('명령 전송에 실패했습니다.'),
              backgroundColor: Colors.red,
            ),
          );
        }
      }
    } catch (e) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('오류가 발생했습니다: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }
}
