import 'package:flutter/material.dart';
import 'dart:async';
import '../services/socket_service.dart';
import 'package:shared_preferences/shared_preferences.dart';

class IoTControlScreen extends StatefulWidget {
  const IoTControlScreen({Key? key}) : super(key: key);

  @override
  State<IoTControlScreen> createState() => _IoTControlScreenState();
}

class _IoTControlScreenState extends State<IoTControlScreen> {
  final SocketService _socketService = SocketService();

  // 장치 상태를 관리하는 변수들
  bool _isBoilerOn = false;
  bool _isLedOn = false;

  // 서버로부터 메시지 리스닝을 위한 변수
  StreamSubscription<String>? _messageSubscription;

  // 로딩 상태
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _connectToServer();
    _loadDeviceStates(); // 저장된 상태 불러오기

    // 서버 메시지 리스닝 설정
    _messageSubscription = _socketService.messageStream.listen(
      _handleServerMessage,
    );
  }

  // 저장된 장치 상태 불러오기
  Future<void> _loadDeviceStates() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      setState(() {
        _isBoilerOn = prefs.getBool('boiler_state') ?? false;
        _isLedOn = prefs.getBool('led_state') ?? false;
      });
    } catch (e) {
      debugPrint('장치 상태 불러오기 오류: $e');
    }
  }

  // 장치 상태 저장
  Future<void> _saveDeviceStates() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setBool('boiler_state', _isBoilerOn);
      await prefs.setBool('led_state', _isLedOn);
    } catch (e) {
      debugPrint('장치 상태 저장 오류: $e');
    }
  }

  @override
  void dispose() {
    _messageSubscription?.cancel();
    super.dispose();
  }

  // 서버로부터 수신한 메시지 처리
  void _handleServerMessage(String message) {
    if (message.contains('@LEDOFF') || message.contains('@LEDOFF')) {
      setState(() {
        _isLedOn = false;
        _saveDeviceStates();
      });
    } else if (message.contains('@LEDON')) {
      setState(() {
        _isLedOn = true;
        _saveDeviceStates();
      });
    } else if (message.contains('@BOILEROFF')) {
      setState(() {
        _isBoilerOn = false;
        _saveDeviceStates();
      });
    } else if (message.contains('@BOILERON')) {
      setState(() {
        _isBoilerOn = true;
        _saveDeviceStates();
      });
    } else if (message.contains('@SOUNDON')) {
      // 이전 호환성을 위해 유지
      setState(() {
        _isBoilerOn = true;
        _isLedOn = true;
        _saveDeviceStates();
      });
    }
  }

  // 서버 연결 함수
  Future<void> _connectToServer() async {
    setState(() {
      _isLoading = true;
    });

    try {
      final connected = await _socketService.connect();
      if (!connected && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('서버 연결에 실패했습니다. 설정을 확인해주세요.'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('서버 연결 오류: $e'), backgroundColor: Colors.red),
        );
      }
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  // IoT 장치 제어 명령 전송
  Future<void> _sendCommand(String command) async {
    setState(() {
      _isLoading = true;
    });

    try {
      // 소켓 연결 확인
      if (!_socketService.isConnected) {
        final connected = await _socketService.connect();
        if (!connected && mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('서버 연결에 실패했습니다. 설정을 확인해주세요.'),
              backgroundColor: Colors.red,
            ),
          );
          setState(() {
            _isLoading = false;
          });
          return;
        }
      }

      // USR_STM32에 명령 전송
      final sendResult = await _socketService.sendMessage("USR_BT:$command");

      if (sendResult && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('명령이 전송되었습니다: $command'),
            backgroundColor: Colors.green,
            duration: const Duration(seconds: 1),
          ),
        );

        // 상태 업데이트
        _updateDeviceStatus(command);
      } else if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('명령 전송에 실패했습니다.'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('명령 전송 오류: $e'), backgroundColor: Colors.red),
        );
      }
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  // 명령에 따른 장치 상태 업데이트
  void _updateDeviceStatus(String command) {
    setState(() {
      if (command == '@BOILEROFF') {
        _isBoilerOn = false;
      } else if (command == '@BOILERON') {
        _isBoilerOn = true;
      } else if (command == '@LEDOFF') {
        _isLedOn = false;
      } else if (command == '@LEDON') {
        _isLedOn = true;
      } else if (command == '@SOUNDON') {
        // 호환성을 위해 유지
        _isBoilerOn = true;
        _isLedOn = true;
      } else if (command == '@SOUNDOFF') {
        // 호환성을 위해 유지
        _isBoilerOn = false;
        _isLedOn = false;
      }
      
      // 상태 변경 후 저장
      _saveDeviceStates();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('IoT 시스템 제어'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.pop(context),
        ),
      ),
      body:
          _isLoading
              ? const Center(child: CircularProgressIndicator())
              : SingleChildScrollView(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    // 상태 카드
                    Card(
                      elevation: 3,
                      child: Padding(
                        padding: const EdgeInsets.all(16.0),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: const [
                            Text(
                              'IoT 시스템 제어',
                              style: TextStyle(
                                fontSize: 20,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            SizedBox(height: 8),
                            Text(
                              '아래 버튼을 눌러 가정 내 IoT 장치를 제어할 수 있습니다. 명령 전송에는 몇 초가 소요될 수 있습니다.',
                              style: TextStyle(
                                fontSize: 14,
                                color: Colors.grey,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                    const SizedBox(height: 24),

                    // 제어 버튼들
                    _buildDeviceControlCard(),
                  ],
                ),
              ),
    );
  }

  Widget _buildDeviceControlCard() {
    return Card(
      elevation: 2,
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              '가정 IoT 장치',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),

            // 보일러 제어
            _buildDeviceToggle(
              icon: Icons.local_fire_department,
              title: '보일러',
              subtitle: _isBoilerOn ? '켜짐' : '꺼짐',
              value: _isBoilerOn,
              onChanged: (value) {
                if (value) {
                  _sendCommand('@BOILERON');
                } else {
                  _sendCommand('@BOILEROFF');
                }
              },
              activeColor: Colors.red,
            ),

            const Divider(),

            // LED 제어
            _buildDeviceToggle(
              icon: Icons.lightbulb,
              title: 'LED',
              subtitle: _isLedOn ? '켜짐' : '꺼짐',
              value: _isLedOn,
              onChanged: (value) {
                if (value) {
                  _sendCommand('@LEDON');
                } else {
                  _sendCommand('@LEDOFF');
                }
              },
              activeColor: Colors.amber,
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDeviceToggle({
    required IconData icon,
    required String title,
    required String subtitle,
    required bool value,
    required ValueChanged<bool> onChanged,
    required Color activeColor,
  }) {
    return ListTile(
      leading: Icon(icon, color: value ? activeColor : Colors.grey, size: 30),
      title: Text(
        title,
        style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
      ),
      subtitle: Text(subtitle),
      trailing: Switch(
        value: value,
        onChanged: _isLoading ? null : onChanged,
        activeColor: activeColor,
      ),
    );
  }
}