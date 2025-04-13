import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../services/socket_service.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({Key? key}) : super(key: key);

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _formKey = GlobalKey<FormState>();
  late TextEditingController _ipController;
  late TextEditingController _portController;
  late TextEditingController _deviceIdController;
  bool _isLoading = true;
  bool _testResult = false;
  bool _isTesting = false;

  @override
  void initState() {
    super.initState();
    _ipController = TextEditingController();
    _portController = TextEditingController();
    _deviceIdController = TextEditingController();
    _loadSettings();
  }

  @override
  void dispose() {
    _ipController.dispose();
    _portController.dispose();
    _deviceIdController.dispose();
    super.dispose();
  }

  // 저장된 설정 로드
  Future<void> _loadSettings() async {
    setState(() {
      _isLoading = true;
    });

    try {
      final prefs = await SharedPreferences.getInstance();

      setState(() {
        _ipController.text = prefs.getString('server_ip') ?? '127.0.0.1';
        _portController.text = (prefs.getInt('server_port') ?? 8080).toString();
        _deviceIdController.text = prefs.getString('device_id') ?? 'USR_AND';
        _isLoading = false;
      });
    } catch (e) {
      debugPrint('설정 로드 오류: $e');
      setState(() {
        _isLoading = false;
      });
    }
  }

  // 설정 저장
  Future<void> _saveSettings() async {
    if (_formKey.currentState!.validate()) {
      setState(() {
        _isLoading = true;
      });

      try {
        final prefs = await SharedPreferences.getInstance();

        await prefs.setString('server_ip', _ipController.text);
        await prefs.setInt('server_port', int.parse(_portController.text));
        await prefs.setString('device_id', _deviceIdController.text);

        // 소켓 서비스 설정 업데이트
        final socketService = SocketService();
        socketService.updateServerConfig(
          _ipController.text,
          int.parse(_portController.text),
          _deviceIdController.text,
        );

        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('설정이 저장되었습니다')));
      } catch (e) {
        debugPrint('설정 저장 오류: $e');
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('설정 저장 오류: $e')));
      } finally {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  // 서버 연결 테스트
  Future<void> _testConnection() async {
    if (_formKey.currentState!.validate()) {
      setState(() {
        _isTesting = true;
      });

      try {
        final socketService = SocketService();
        socketService.updateServerConfig(
          _ipController.text,
          int.parse(_portController.text),
          _deviceIdController.text,
        );

        final result = await socketService.connect();

        setState(() {
          _testResult = result;
          _isTesting = false;
        });

        // 결과 표시
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(result ? '서버 연결 성공!' : '서버 연결 실패'),
            backgroundColor: result ? Colors.green : Colors.red,
          ),
        );

        // 테스트 후 연결 종료
        socketService.disconnect();
      } catch (e) {
        debugPrint('연결 테스트 오류: $e');
        setState(() {
          _testResult = false;
          _isTesting = false;
        });

        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('연결 테스트 오류: $e'), backgroundColor: Colors.red),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return Scaffold(
        appBar: AppBar(title: Text('설정')),
        body: Center(child: CircularProgressIndicator()),
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('설정'),
        actions: [
          IconButton(icon: const Icon(Icons.save), onPressed: _saveSettings),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Form(
          key: _formKey,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                '서버 연결 설정',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 16),

              // 서버 IP 주소
              TextFormField(
                controller: _ipController,
                decoration: const InputDecoration(
                  labelText: '서버 IP 주소',
                  border: OutlineInputBorder(),
                  hintText: '예: 192.168.0.100',
                ),
                keyboardType: TextInputType.text,
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'IP 주소를 입력해주세요';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),

              // 서버 포트
              TextFormField(
                controller: _portController,
                decoration: const InputDecoration(
                  labelText: '서버 포트',
                  border: OutlineInputBorder(),
                  hintText: '예: 8080',
                ),
                keyboardType: TextInputType.number,
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return '포트를 입력해주세요';
                  }
                  if (int.tryParse(value) == null) {
                    return '유효한 포트 번호를 입력해주세요';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),

              // 디바이스 ID
              TextFormField(
                controller: _deviceIdController,
                decoration: const InputDecoration(
                  labelText: '디바이스 ID',
                  border: OutlineInputBorder(),
                  hintText: '예: USR_AND',
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return '디바이스 ID를 입력해주세요';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 24),

              // 연결 테스트 버튼
              SizedBox(
                width: double.infinity,
                child: ElevatedButton(
                  onPressed: _isTesting ? null : _testConnection,
                  child:
                      _isTesting
                          ? const SizedBox(
                            height: 20,
                            width: 20,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                          : const Text('서버 연결 테스트'),
                ),
              ),

              // 연결 상태 표시
              if (_isTesting)
                const Padding(
                  padding: EdgeInsets.only(top: 16.0),
                  child: Center(child: Text('연결 시도 중...')),
                ),
            ],
          ),
        ),
      ),
    );
  }
}