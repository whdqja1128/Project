import 'package:flutter/material.dart';
import '../services/socket_service.dart';
import 'package:shared_preferences/shared_preferences.dart';

class AlarmScreen extends StatefulWidget {
  const AlarmScreen({Key? key}) : super(key: key);

  @override
  State<AlarmScreen> createState() => _AlarmScreenState();
}

class _AlarmScreenState extends State<AlarmScreen> {
  final SocketService _socketService = SocketService();

  // 알람 관련 변수들
  List<TimeOfDay> _alarmTimes = [
    const TimeOfDay(hour: 16, minute: 47),
    const TimeOfDay(hour: 16, minute: 48),
    const TimeOfDay(hour: 18, minute: 46),
  ];

  // 로딩 상태
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _connectToServer();
    _loadSavedAlarms(); // 저장된 알람 불러오기
  }

  // 저장된 알람 불러오기
  Future<void> _loadSavedAlarms() async {
    setState(() {
      _isLoading = true;
    });

    try {
      final prefs = await SharedPreferences.getInstance();
      final alarmCount = prefs.getInt('alarm_count') ?? 0;

      if (alarmCount > 0) {
        final loadedAlarms = <TimeOfDay>[];

        for (int i = 0; i < alarmCount; i++) {
          final hour = prefs.getInt('alarm_${i}_hour') ?? 0;
          final minute = prefs.getInt('alarm_${i}_minute') ?? 0;

          loadedAlarms.add(TimeOfDay(hour: hour, minute: minute));
        }

        if (loadedAlarms.isNotEmpty) {
          setState(() {
            _alarmTimes = loadedAlarms;
          });
        }
      }
    } catch (e) {
      debugPrint('알람 로드 오류: $e');
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  // 알람 저장
  Future<void> _saveAlarms() async {
    try {
      final prefs = await SharedPreferences.getInstance();

      // 저장된 알람 수 설정
      await prefs.setInt('alarm_count', _alarmTimes.length);

      // 각 알람 저장
      for (int i = 0; i < _alarmTimes.length; i++) {
        await prefs.setInt('alarm_${i}_hour', _alarmTimes[i].hour);
        await prefs.setInt('alarm_${i}_minute', _alarmTimes[i].minute);
      }

      debugPrint('알람 저장 완료: ${_alarmTimes.length}개');
    } catch (e) {
      debugPrint('알람 저장 오류: $e');
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

  // 알람 시간 설정 명령 전송
  Future<void> _sendAlarmSetCommand(int index, TimeOfDay time) async {
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

      // Set_Alarm_Time 함수 직접 호출을 위한 명령 형식
      // 예: ALARM_SET@0@16@47 -> 0번 알람을 16시 47분으로 설정
      String command = "ALARM_SET@${index}@${time.hour}@${time.minute}";

      // STM32에 명령 전송
      final sendResult = await _socketService.sendMessage("USR_STM32:$command");

      if (sendResult && mounted) {
        // 알람 시간 업데이트
        setState(() {
          if (index < _alarmTimes.length) {
            _alarmTimes[index] = time;
          } else {
            // 인덱스가 범위를 벗어날 경우 새 알람 추가 (백엔드는 Set_Alarm_Time에서 처리)
            _alarmTimes.add(time);
          }
        });

        // 알람 저장
        await _saveAlarms();

        // 성공 메시지 표시
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('알람 ${index + 1}번 설정: ${time.hour}시 ${time.minute}분'),
            backgroundColor: Colors.green,
            duration: const Duration(seconds: 2),
          ),
        );
      } else if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('알람 설정 명령 전송에 실패했습니다.'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('알람 설정 오류: $e'), backgroundColor: Colors.red),
        );
      }
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  // 알람 삭제 명령 전송
  Future<void> _sendAlarmDeleteCommand(int index) async {
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

      // 알람 삭제 명령 형식: ALARM_DELETE@인덱스
      String command = "ALARM_DELETE@${index}";

      // STM32에 명령 전송
      final sendResult = await _socketService.sendMessage("USR_STM32:$command");

      if (sendResult && mounted) {
        // 알람 목록에서 제거
        setState(() {
          if (index < _alarmTimes.length) {
            _alarmTimes.removeAt(index);
          }
        });

        // 알람 저장
        await _saveAlarms();

        // 성공 메시지 표시
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('알람 ${index + 1}번이 삭제되었습니다'),
            backgroundColor: Colors.green,
            duration: const Duration(seconds: 2),
          ),
        );
      } else if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('알람 삭제 명령 전송에 실패했습니다.'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('알람 삭제 오류: $e'), backgroundColor: Colors.red),
        );
      }
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('알람 설정'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.pop(context),
        ),
        actions: [
          // 알람 추가 버튼
          IconButton(
            icon: const Icon(Icons.add_alarm),
            onPressed: _isLoading ? null : _addNewAlarm,
            tooltip: '새 알람 추가',
          ),
        ],
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
                              '알람 설정',
                              style: TextStyle(
                                fontSize: 20,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            SizedBox(height: 8),
                            Text(
                              '아래에서 알람 시간을 설정하거나 삭제할 수 있습니다. 설정된 시간에 알림이 울립니다.',
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

                    // 알람 설정 카드
                    _buildAlarmSettingsCard(),
                  ],
                ),
              ),
    );
  }

  // 새 알람 추가
  void _addNewAlarm() async {
    // 기본값으로 현재 시간 설정
    final currentTime = TimeOfDay.now();

    // 시간 선택 대화상자 표시
    final TimeOfDay? picked = await showTimePicker(
      context: context,
      initialTime: currentTime,
    );

    if (picked != null && mounted) {
      // 새 알람 인덱스는 현재 목록 길이
      int newIndex = _alarmTimes.length;

      // 알람 설정 명령 전송
      await _sendAlarmSetCommand(newIndex, picked);
    }
  }

  Widget _buildAlarmSettingsCard() {
    if (_alarmTimes.isEmpty) {
      return Card(
        elevation: 2,
        child: Padding(
          padding: const EdgeInsets.all(16.0),
          child: Center(
            child: Column(
              children: const [
                Icon(Icons.alarm_off, size: 48, color: Colors.grey),
                SizedBox(height: 16),
                Text(
                  '설정된 알람이 없습니다',
                  style: TextStyle(fontSize: 16, color: Colors.grey),
                ),
                SizedBox(height: 8),
                Text(
                  '화면 상단의 + 버튼을 눌러 새 알람을 추가하세요',
                  style: TextStyle(fontSize: 14, color: Colors.grey),
                ),
              ],
            ),
          ),
        ),
      );
    }

    return Card(
      elevation: 2,
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              '알람 목록',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),

            // 알람 목록 동적 생성
            for (int i = 0; i < _alarmTimes.length; i++) ...[
              _buildAlarmSetting(i, '알람 ${i + 1}'),
              if (i < _alarmTimes.length - 1) const Divider(),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildAlarmSetting(int index, String title) {
    final time = _alarmTimes[index];
    final formattedTime =
        '${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}';

    return ListTile(
      leading: const Icon(Icons.alarm, color: Colors.blue, size: 30),
      title: Text(
        title,
        style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
      ),
      subtitle: Text('설정된 시간: $formattedTime'),
      trailing: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          // 시간 설정 버튼
          TextButton(
            onPressed: _isLoading ? null : () => _selectTime(context, index),
            child: const Text('시간 설정'),
          ),
          // 삭제 버튼
          IconButton(
            icon: const Icon(Icons.delete, color: Colors.red),
            onPressed: _isLoading ? null : () => _confirmDelete(index),
            tooltip: '알람 삭제',
          ),
        ],
      ),
    );
  }

  Future<void> _selectTime(BuildContext context, int index) async {
    final TimeOfDay? picked = await showTimePicker(
      context: context,
      initialTime: _alarmTimes[index],
    );

    if (picked != null && mounted) {
      // 알람 설정 명령 전송
      await _sendAlarmSetCommand(index, picked);
    }
  }

  Future<void> _confirmDelete(int index) async {
    // 삭제 확인 대화상자 표시
    bool? confirm = await showDialog<bool>(
      context: context,
      builder:
          (context) => AlertDialog(
            title: Text('알람 ${index + 1} 삭제'),
            content: const Text('이 알람을 삭제하시겠습니까?'),
            actions: [
              TextButton(
                onPressed: () => Navigator.of(context).pop(false),
                child: const Text('취소'),
              ),
              TextButton(
                onPressed: () => Navigator.of(context).pop(true),
                child: const Text('삭제', style: TextStyle(color: Colors.red)),
              ),
            ],
          ),
    );

    if (confirm == true && mounted) {
      await _sendAlarmDeleteCommand(index);
    }
  }
}
