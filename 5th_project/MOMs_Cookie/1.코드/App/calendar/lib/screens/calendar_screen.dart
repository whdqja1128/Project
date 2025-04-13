import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:table_calendar/table_calendar.dart';
import 'package:intl/intl.dart';
import '../models/event.dart';
import '../services/event_provider.dart';
import 'event_screen.dart';
import 'settings_screen.dart';

class CalendarScreen extends StatefulWidget {
  const CalendarScreen({super.key});

  @override
  State<CalendarScreen> createState() => _CalendarScreenState();
}

class _CalendarScreenState extends State<CalendarScreen> {
  late CalendarFormat _calendarFormat;
  late DateTime _focusedDay;
  late DateTime _selectedDay;
  List<Event> _selectedEvents = []; // 선택된 날짜의 이벤트를 저장하는 리스트
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _calendarFormat = CalendarFormat.month;
    _focusedDay = DateTime.now();
    _selectedDay = DateTime.now();

    // 앱 시작 시 일정 로딩
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) {
        Provider.of<EventProvider>(context, listen: false).syncPendingEvents();
        _loadSelectedDayEvents();
      }
    });
  }

  // 선택된 날짜의 이벤트 로딩
  void _loadSelectedDayEvents() {
    setState(() {
      _isLoading = true;
    });

    final provider = Provider.of<EventProvider>(context, listen: false);
    _selectedEvents = provider.getEventsForDay(_selectedDay);

    setState(() {
      _isLoading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('일정 관리'),
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.pop(context),
        ),
        actions: [
          // 일정 저장 버튼
          IconButton(
            icon: const Icon(Icons.save_alt),
            onPressed: () async {
              // 저장 중임을 표시하는 다이얼로그
              showDialog(
                context: context,
                barrierDismissible: false,
                builder: (BuildContext dialogContext) => const AlertDialog(
                  content: Row(
                    children: [
                      CircularProgressIndicator(),
                      SizedBox(width: 16),
                      Text('일정 파일 저장 중...'),
                    ],
                  ),
                ),
              );

              // 일정 파일 저장 실행
              final success = await Provider.of<EventProvider>(
                context, 
                listen: false
              ).manualSaveScheduleFile();

              // 다이얼로그 닫기 (context가 mounted 상태인지 확인)
              if (mounted) {
                Navigator.of(context).pop();

                // 결과 알림
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(
                    content: Text(
                      success ? '일정 파일이 저장되었습니다' : '일정 파일 저장 실패',
                    ),
                    backgroundColor: success ? Colors.green : Colors.red,
                    duration: const Duration(seconds: 3),
                  ),
                );
              }
            },
            tooltip: '일정을 파일로 저장',
          ),
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () {
              Navigator.push(
                context,
                MaterialPageRoute(builder: (context) => const SettingsScreen()),
              );
            },
            tooltip: '설정',
          ),
        ],
      ),
      body: Column(
        children: [
          _buildCalendar(),
          const SizedBox(height: 8.0),
          const Divider(height: 1),
          Expanded(child: _buildEventList()),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => _navigateToEventScreen(null),
        child: const Icon(Icons.add),
        tooltip: '일정 추가',
      ),
    );
  }

  Widget _buildCalendar() {
    return Consumer<EventProvider>(
      builder: (context, provider, _) {
        return TableCalendar<Event>(
          firstDay: DateTime.utc(2020, 1, 1),
          lastDay: DateTime.utc(2030, 12, 31),
          focusedDay: _focusedDay,
          calendarFormat: _calendarFormat,
          selectedDayPredicate: (day) => isSameDay(_selectedDay, day),
          eventLoader: (day) {
            // 직접 이벤트 반환 (Future 타입이 아닌 List<Event> 타입으로 반환)
            return provider.getEventsForDay(day);
          },
          onDaySelected: (selectedDay, focusedDay) {
            setState(() {
              _selectedDay = selectedDay;
              _focusedDay = focusedDay;
              // 날짜 선택 시 해당 날짜의 이벤트 로드
              _selectedEvents = provider.getEventsForDay(selectedDay);
            });
          },
          onFormatChanged: (format) {
            setState(() {
              _calendarFormat = format;
            });
          },
          onPageChanged: (focusedDay) {
            _focusedDay = focusedDay;
          },
          calendarStyle: CalendarStyle(
            todayDecoration: BoxDecoration(
              color: Colors.blue.withOpacity(0.5),
              shape: BoxShape.circle,
            ),
            selectedDecoration: const BoxDecoration(
              color: Colors.blue,
              shape: BoxShape.circle,
            ),
            markerDecoration: const BoxDecoration(
              color: Colors.red,
              shape: BoxShape.circle,
            ),
          ),
          headerStyle: const HeaderStyle(
            formatButtonVisible: true,
            titleCentered: true,
          ),
        );
      },
    );
  }

  Widget _buildEventList() {
    if (_isLoading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (_selectedEvents.isEmpty) {
      return const Center(child: Text('이 날짜에 일정이 없습니다.'));
    }

    return ListView.builder(
      itemCount: _selectedEvents.length,
      itemBuilder: (context, index) {
        final event = _selectedEvents[index];

        // 우선순위에 따른 색상 설정
        Color priorityColor;
        switch (event.priority) {
          case 1: // 낮음
            priorityColor = Colors.green;
            break;
          case 2: // 중간
            priorityColor = Colors.orange;
            break;
          case 3: // 높음
            priorityColor = Colors.red;
            break;
          default:
            priorityColor = Colors.grey;
        }

        // 반복 일정 아이콘 선택
        IconData? repeatIcon;
        String repeatText = '';
        if (event.isRepeating) {
          switch (event.repeatType) {
            case 'weekly':
              repeatIcon = Icons.repeat_one;
              repeatText = '매주';
              break;
            case 'monthly':
              repeatIcon = Icons.date_range;
              repeatText = '매월';
              break;
            case 'yearly':
              repeatIcon = Icons.calendar_today;
              repeatText = '매년';
              break;
          }
        }

        return Card(
          margin: const EdgeInsets.symmetric(horizontal: 8.0, vertical: 4.0),
          child: ListTile(
            leading: Container(
              width: 12,
              height: double.infinity,
              color: priorityColor,
            ),
            title: Row(
              children: [
                Expanded(child: Text(event.title)),
                if (event.isRepeating)
                  Tooltip(
                    message: repeatText,
                    child: Icon(repeatIcon, size: 18, color: Colors.blue),
                  ),
              ],
            ),
            subtitle: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (event.description.isNotEmpty) Text(event.description),
                Text(
                  event.isAllDay
                      ? '종일 일정'
                      : '${_formatTime(event.startTime)} - ${_formatTime(event.endTime)}',
                  style: TextStyle(fontSize: 12),
                ),
                if (event.isRepeating)
                  Text(
                    repeatText,
                    style: TextStyle(fontSize: 12, color: Colors.blue),
                  ),
              ],
            ),
            trailing: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                // 동기화 상태 표시
                Icon(
                  event.synced ? Icons.cloud_done : Icons.cloud_upload,
                  color: event.synced ? Colors.green : Colors.grey,
                  size: 20,
                ),
                const SizedBox(width: 8),
                // 삭제 버튼
                IconButton(
                  icon: const Icon(Icons.delete, color: Colors.red),
                  onPressed: () => _deleteEvent(event),
                ),
              ],
            ),
            onTap: () => _navigateToEventScreen(event),
          ),
        );
      },
    );
  }

  String _formatTime(DateTime? time) {
    if (time == null) return '';
    return DateFormat('HH:mm').format(time);
  }

  // BuildContext 비동기 사용 오류 수정
  Future<void> _navigateToEventScreen(Event? event) async {
    final BuildContext currentContext = context;
    if (!mounted) return;

    final result = await Navigator.push(
      currentContext,
      MaterialPageRoute(
        builder: (_) => EventScreen(selectedDate: _selectedDay, event: event),
      ),
    );

    // 새로운 이벤트가 추가되었거나 기존 이벤트가 수정되었을 경우
    if (result == true && mounted) {
      _loadSelectedDayEvents();
    }
  }

  void _deleteEvent(Event event) {
    final BuildContext currentContext = context;

    showDialog(
      context: currentContext,
      builder:
          (dialogContext) => AlertDialog(
            title: const Text('일정 삭제'),
            content: const Text('이 일정을 삭제하시겠습니까?'),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(dialogContext),
                child: const Text('취소'),
              ),
              TextButton(
                onPressed: () {
                  Navigator.pop(dialogContext);
                  if (mounted) {
                    Provider.of<EventProvider>(
                      currentContext,
                      listen: false,
                    ).deleteEvent(event);
                    _loadSelectedDayEvents(); // 삭제 후 이벤트 목록 갱신
                  }
                },
                child: const Text('삭제', style: TextStyle(color: Colors.red)),
              ),
            ],
          ),
    );
  }
}