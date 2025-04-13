import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:path_provider/path_provider.dart';

class SocketService {
  static final SocketService _instance = SocketService._internal();
  factory SocketService() => _instance;
  SocketService._internal();

  Socket? _socket;
  final _messageController = StreamController<String>.broadcast();
  bool _isConnected = false;

  // 서버 설정 (기본값)
  String _serverIp = '127.0.0.1';
  int _serverPort = 8080;
  String _deviceId = 'USR_AND'; // 서버가 인식하는 클라이언트 ID

  Stream<String> get messageStream => _messageController.stream;
  bool get isConnected => _isConnected;

  // 설정 로드
  Future<void> loadSettings() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      _serverIp = prefs.getString('server_ip') ?? _serverIp;
      _serverPort = prefs.getInt('server_port') ?? _serverPort;
      _deviceId = prefs.getString('device_id') ?? _deviceId;
    } catch (e) {
      debugPrint('설정 로드 오류: $e');
    }
  }

  // 서버에 연결
  Future<bool> connect() async {
    if (_isConnected) {
      return true;
    }

    try {
      await loadSettings();

      // 서버에 소켓 연결
      _socket = await Socket.connect(
        _serverIp,
        _serverPort,
        timeout: const Duration(seconds: 5),
      );
      _isConnected = true;

      // 인증 메시지 전송
      _socket!.write(_deviceId);

      // 소켓 이벤트 리스너 설정
      _socket!.listen(
        (data) {
          // 데이터 수신
          final message = utf8.decode(data);
          _messageController.add(message);
          debugPrint('서버로부터 메시지: $message');

          // 일정 관련 메시지 수신 처리
          _handleIncomingMessage(message);
        },
        onError: (error) {
          debugPrint('소켓 오류: $error');
          disconnect();
        },
        onDone: () {
          debugPrint('서버 연결 종료');
          disconnect();
        },
      );

      return true;
    } catch (e) {
      debugPrint('서버 연결 실패: $e');
      _isConnected = false;
      return false;
    }
  }

  // 수신된 메시지 처리
  void _handleIncomingMessage(String message) {
    // 서버로부터 수신된 메시지 처리
    debugPrint('서버로부터 메시지 처리: $message');

    // USR_AI로부터 '일정' 요청 감지
    if (message.contains('[USR_AI]일정') || 
        message.contains('일정정보') ||
        (message.contains('USR_AI') && message.contains('일정'))) {
      debugPrint('USR_AI로부터 일정 요청 감지: 일정 데이터 서버 전송 시작');
      _sendScheduleToServer();
    }
  }

  // 일정 데이터를 서버에 전송
  Future<bool> _sendScheduleToServer() async {
    try {
      if (!_isConnected) {
        final connected = await connect();
        if (!connected) {
          debugPrint('서버 연결 실패: 일정 전송 불가');
          return false;
        }
      }

      // 현재 일정 데이터 가져오기
      final scheduleData = await _getScheduleData();
      
      // 서버에 일정 파일 전송 시작 알림
      await sendMessage("Weather:SCHEDULE_FILE_START");
      await Future.delayed(const Duration(milliseconds: 300)); // 서버 처리 시간 제공
      
      // 일정 데이터 라인별로 전송
      final lines = scheduleData.split('\n');
      for (final line in lines.where((line) => line.trim().isNotEmpty)) {
        await sendMessage("Weather:SCHEDULE_FILE_CONTENT:$line");
        await Future.delayed(const Duration(milliseconds: 100)); // 전송 간격
      }
      
      // 서버에 일정 전송 완료 알림
      await sendMessage("Weather:SCHEDULE_FILE_END");
      
      debugPrint('일정 데이터 서버 전송 완료');
      return true;
    } catch (e) {
      debugPrint('일정 서버 전송 오류: $e');
      return false;
    }
  }

  // 일정 데이터 생성 (오늘/내일 일정만 포함)
  Future<String> _getScheduleData() async {
    try {
      // 앱 디렉토리 가져오기
      final directory = await getApplicationDocumentsDirectory();
      final scheduleFile = File('${directory.path}/schedule.txt');

      // 현재 날짜 가져오기
      final now = DateTime.now();
      final today = now.toLocal();
      final formattedDate = '${today.year}년 ${today.month}월 ${today.day}일 ${today.hour}시 ${today.minute}분';
      
      // 앱에서 일정을 가져오는 로직...
      // 실제로는 EventProvider를 통해 가져와야 하지만 여기서는 간단히 구현
      
      // schedule.txt 파일이 존재하면 그 내용을 읽기
      if (await scheduleFile.exists()) {
        final content = await scheduleFile.readAsString();

        // 내용이 없는 경우
        if (content.trim().isEmpty) {
          return "일정\n$formattedDate 기준\n현재 등록된 일정이 없습니다.";
        }

        // 파일에 이미 "일정" 행이 포함된 경우 그대로 반환
        if (content.trim().startsWith("일정")) {
          return content;
        } else {
          // "일정" 행이 없으면 추가
          final result = "일정\n$formattedDate 기준\n$content";
          return result;
        }
      } else {
        // 파일이 없는 경우
        return "일정\n$formattedDate 기준\n현재 등록된 일정이 없습니다.";
      }
    } catch (e) {
      debugPrint('일정 데이터 생성 오류: $e');
      return "일정\n일정 정보를 가져오는 중 오류가 발생했습니다.";
    }
  }

  // 연결 종료
  void disconnect() {
    _socket?.destroy();
    _socket = null;
    _isConnected = false;
  }

  // 메시지 전송
  Future<bool> sendMessage(String message) async {
    if (!_isConnected) {
      final connected = await connect();
      if (!connected) {
        return false;
      }
    }

    try {
      debugPrint('전송 메시지: $message');
      _socket!.write(message);
      return true;
    } catch (e) {
      debugPrint('메시지 전송 오류: $e');
      disconnect();
      return false;
    }
  }

  // 서버 설정 변경
  void updateServerConfig(String ip, int port, String deviceId) {
    _serverIp = ip;
    _serverPort = port;
    _deviceId = deviceId;

    // 설정 저장
    SharedPreferences.getInstance().then((prefs) {
      prefs.setString('server_ip', ip);
      prefs.setInt('server_port', port);
      prefs.setString('device_id', deviceId);
    });

    // 이미 연결되어 있다면 재연결
    if (_isConnected) {
      disconnect();
      connect();
    }
  }

  // 리소스 해제
  void dispose() {
    disconnect();
    _messageController.close();
  }

  // 수동으로 현재 일정을 서버에 전송 (외부에서 호출 가능)
  Future<bool> saveScheduleToFile() async {
    return await _sendScheduleToServer();
  }
}