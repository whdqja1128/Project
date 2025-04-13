import 'dart:io';
import 'package:flutter/material.dart';
import '../models/event.dart';
import 'database.dart';
import 'socket_service.dart';
import 'package:path_provider/path_provider.dart';
import 'package:intl/intl.dart';

class EventProvider extends ChangeNotifier {
  final DatabaseService _databaseService = DatabaseService();
  final SocketService _socketService = SocketService();
  final Map<DateTime, List<Event>> _events = {};
  bool _isLoading = false;

  EventProvider() {
    _fetchEvents();
    _removeExpiredEvents();
    _saveLocalScheduleFile(); // 로컬 파일 저장 (서버 전송용)
  }

  Map<DateTime, List<Event>> get events => _events;
  bool get isLoading => _isLoading;

  // 이벤트 데이터를 로드
  Future<void> _fetchEvents() async {
    _isLoading = true;
    notifyListeners();

    try {
      final events = await _databaseService.getEvents();
      _events.clear();

      for (final event in events) {
        final date = DateTime(
          event.date.year,
          event.date.month,
          event.date.day,
        );
        if (_events[date] != null) {
          _events[date]!.add(event);
        } else {
          _events[date] = [event];
        }
      }
    } catch (e) {
      debugPrint('이벤트 로드 오류: $e');
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  // 지난 일정 제거
  Future<void> _removeExpiredEvents() async {
    try {
      final today = DateTime.now();
      final todayDate = DateTime(today.year, today.month, today.day);

      // 로컬 DB에서 만료된 이벤트 삭제
      final events = await _databaseService.getEvents();

      for (final event in events) {
        final eventDate = DateTime(
          event.date.year,
          event.date.month,
          event.date.day,
        );
        if (eventDate.isBefore(todayDate)) {
          await _databaseService.deleteEvent(event.id);
          debugPrint('만료된 일정 삭제: ${event.title} (${event.date})');
        }
      }

      // 메모리에서도 지난 일정 제거
      final expiredDates =
          _events.keys.where((date) => date.isBefore(todayDate)).toList();

      for (final date in expiredDates) {
        _events.remove(date);
      }

      notifyListeners();
    } catch (e) {
      debugPrint('지난 일정 제거 오류: $e');
    }
  }

  // 일정을 앱 내부 저장소에 저장 (로컬 파일)
  Future<bool> _saveLocalScheduleFile() async {
    try {
      // 앱 내부 저장소 경로 가져오기
      final directory = await getApplicationDocumentsDirectory();
      final file = File('${directory.path}/schedule.txt');

      // 일정 데이터 생성
      final content = await _createScheduleContent();

      // 파일에 저장
      await file.writeAsString(content);
      debugPrint('로컬 일정 파일 저장 완료: ${file.path}');
      return true;
    } catch (e) {
      debugPrint('로컬 일정 파일 저장 오류: $e');
      return false;
    }
  }

  // 일정 내용 생성 함수
  Future<String> _createScheduleContent() async {
    // 모든 이벤트 수집
    final allEvents = <Event>[];
    _events.forEach((date, eventsForDay) {
      allEvents.addAll(eventsForDay);
    });

    // 날짜순으로 정렬
    allEvents.sort((a, b) => a.date.compareTo(b.date));

    // 현재 날짜 가져오기
    final now = DateTime.now();
    final today = DateTime(now.year, now.month, now.day);
    final tomorrow = today.add(const Duration(days: 1));
    final formattedDate =
        '${now.year}년 ${now.month}월 ${now.day}일 ${now.hour}시 ${now.minute}분';

    // 오늘과 내일의 일정만 필터링
    final filteredEvents =
        allEvents.where((event) {
          final eventDate = DateTime(
            event.date.year,
            event.date.month,
            event.date.day,
          );
          return eventDate.isAtSameMomentAs(today) ||
              eventDate.isAtSameMomentAs(tomorrow);
        }).toList();

    // 중복 제거 (제목, 날짜가 동일한 경우 중복으로 간주)
    final uniqueEvents = <Event>[];
    final uniqueKeys = <String>{};

    for (final event in filteredEvents) {
      final eventKey =
          '${event.title}_${event.date.year}-${event.date.month}-${event.date.day}';

      if (!uniqueKeys.contains(eventKey)) {
        uniqueKeys.add(eventKey);
        uniqueEvents.add(event);
      }
    }

    // 날짜가 빠른 순으로 정렬 후, 같은 날짜에서는 중요도가 높은 순으로 정렬
    uniqueEvents.sort((a, b) {
      // 먼저 날짜로 비교
      final dateComparison = a.date.compareTo(b.date);
      if (dateComparison != 0) {
        return dateComparison; // 날짜가 다르면 날짜순
      }

      // 날짜가 같으면 중요도 역순으로 비교 (높은 중요도가 먼저 오도록)
      return b.priority.compareTo(a.priority);
    });

    // 파일에 쓰기 (일정 날짜 / 제목 / 설명 / 중요도 형식)
    final buffer = StringBuffer();
    // 맨 위에 "일정" 문자열 추가
    buffer.writeln("일정");
    buffer.writeln("$formattedDate 기준");
    buffer.writeln("");

    // 오늘/내일 일정이 없는 경우 메시지 추가
    if (uniqueEvents.isEmpty) {
      buffer.writeln("오늘/내일 예정된 일정이 없습니다.");
    } else {
      // 오늘 일정
      final todayEvents =
          uniqueEvents
              .where(
                (e) => DateTime(
                  e.date.year,
                  e.date.month,
                  e.date.day,
                ).isAtSameMomentAs(today),
              )
              .toList();

      if (todayEvents.isNotEmpty) {
        buffer.writeln("== 오늘 일정 ==");
        for (final event in todayEvents) {
          _writeEventLine(buffer, event);
        }
        buffer.writeln("");
      }

      // 내일 일정
      final tomorrowEvents =
          uniqueEvents
              .where(
                (e) => DateTime(
                  e.date.year,
                  e.date.month,
                  e.date.day,
                ).isAtSameMomentAs(tomorrow),
              )
              .toList();

      if (tomorrowEvents.isNotEmpty) {
        buffer.writeln("== 내일 일정 ==");
        for (final event in tomorrowEvents) {
          _writeEventLine(buffer, event);
        }
      }
    }

    return buffer.toString();
  }

  // 이벤트 한 줄 포맷팅
  void _writeEventLine(StringBuffer buffer, Event event) {
    // 날짜 포맷을 yyyy/MM/dd 형식으로
    final dateStr = '${event.date.year}/${event.date.month}/${event.date.day}';

    // 설명이 비어있으면 "-"로 대체
    final description = event.description.isEmpty ? "-" : event.description;

    // 우선순위 텍스트 변환
    String priorityText;
    switch (event.priority) {
      case 1:
        priorityText = "낮음";
        break;
      case 2:
        priorityText = "중간";
        break;
      case 3:
        priorityText = "높음";
        break;
      default:
        priorityText = "중간";
    }

    // 형식: 일정 날짜 / 제목 / 설명 / 중요도
    buffer.writeln('$dateStr / ${event.title} / $description / $priorityText');
  }

  // 특정 날짜의 이벤트 조회
  List<Event> getEventsForDay(DateTime day) {
    final date = DateTime(day.year, day.month, day.day);
    return _events[date] ?? [];
  }

  // 이벤트 추가
  Future<void> addEvent(Event event) async {
    try {
      await _databaseService.insertEvent(event);

      final date = DateTime(event.date.year, event.date.month, event.date.day);
      if (_events[date] != null) {
        _events[date]!.add(event);
      } else {
        _events[date] = [event];
      }

      notifyListeners();

      // 일정 추가 시 로컬 파일 업데이트
      await _saveLocalScheduleFile();
    } catch (e) {
      debugPrint('이벤트 추가 오류: $e');
    }
  }

  // 이벤트 업데이트
  Future<void> updateEvent(Event oldEvent, Event newEvent) async {
    try {
      await _databaseService.updateEvent(newEvent);

      final oldDate = DateTime(
        oldEvent.date.year,
        oldEvent.date.month,
        oldEvent.date.day,
      );
      final newDate = DateTime(
        newEvent.date.year,
        newEvent.date.month,
        newEvent.date.day,
      );

      // 같은 날짜인 경우
      if (oldDate == newDate) {
        final index = _events[oldDate]!.indexWhere((e) => e.id == oldEvent.id);
        if (index != -1) {
          _events[oldDate]![index] = newEvent;
        }
      } else {
        // 다른 날짜인 경우
        _events[oldDate]?.removeWhere((e) => e.id == oldEvent.id);
        if (_events[oldDate]?.isEmpty ?? false) {
          _events.remove(oldDate);
        }

        if (_events[newDate] != null) {
          _events[newDate]!.add(newEvent);
        } else {
          _events[newDate] = [newEvent];
        }
      }

      notifyListeners();

      // 일정 업데이트 시 로컬 파일 업데이트
      await _saveLocalScheduleFile();
    } catch (e) {
      debugPrint('이벤트 업데이트 오류: $e');
    }
  }

  // 이벤트 삭제
  Future<void> deleteEvent(Event event) async {
    try {
      await _databaseService.deleteEvent(event.id);

      final date = DateTime(event.date.year, event.date.month, event.date.day);
      _events[date]?.removeWhere((e) => e.id == event.id);

      if (_events[date]?.isEmpty ?? false) {
        _events.remove(date);
      }

      notifyListeners();

      // 일정 삭제 시 로컬 파일 업데이트
      await _saveLocalScheduleFile();
    } catch (e) {
      debugPrint('이벤트 삭제 오류: $e');
    }
  }

  // 미동기화된 이벤트 동기화 시도
  Future<Map<String, dynamic>> syncPendingEvents() async {
    // 로컬 파일 업데이트
    final success = await _saveLocalScheduleFile();
    return {
      'success': success,
      'message': success ? '일정 파일 업데이트 완료' : '일정 파일 업데이트 실패',
    };
  }

  // 이벤트 새로고침
  Future<void> refreshEvents() async {
    await _fetchEvents();
    await _removeExpiredEvents();
    await _saveLocalScheduleFile();
  }

  // 수동으로 일정 파일 생성 및 서버 전송 (UI에서 호출 가능)
  Future<bool> manualSaveScheduleFile() async {
    // 1. 먼저 로컬 파일 업데이트
    await _saveLocalScheduleFile();

    // 2. 서버로 일정 전송
    return await _socketService.saveScheduleToFile();
  }
}