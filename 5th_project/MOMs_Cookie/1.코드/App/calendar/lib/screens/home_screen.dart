import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:table_calendar/table_calendar.dart';
import 'package:intl/intl.dart';
import '../models/event.dart';
import '../services/event_provider.dart';
import 'event_screen.dart';
import 'settings_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({Key? key}) : super(key: key);

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late CalendarFormat _calendarFormat;
  late DateTime _focusedDay;
  late DateTime _selectedDay;

  @override
  void initState() {
    super.initState();
    _calendarFormat = CalendarFormat.month;
    _focusedDay = DateTime.now();
    _selectedDay = DateTime.now();
    
    // 앱 시작 시 미동기화 이벤트 동기화 시도
    WidgetsBinding.instance.addPostFrameCallback((_) {
      Provider.of<EventProvider>(context, listen: false).syncPendingEvents();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('IoT 캘린더'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () {
              Provider.of<EventProvider>(context, listen: false).refreshEvents();
            },
            tooltip: '새로고침',
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
        return TableCalendar(
          firstDay: DateTime.utc(2020, 1, 1),
          lastDay: DateTime.utc(2030, 12, 31),
          focusedDay: _focusedDay,
          calendarFormat: _calendarFormat,
          selectedDayPredicate: (day) => isSameDay(_selectedDay, day),
          eventLoader: (day) => provider.getEventsForDay(day),
          onDaySelected: (selectedDay, focusedDay) {
            setState(() {
              _selectedDay = selectedDay;
              _focusedDay = focusedDay;
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
    return Consumer<EventProvider>(
      builder: (context, provider, _) {
        final events = provider.getEventsForDay(_selectedDay);
        
        if (events.isEmpty) {
          return const Center(
            child: Text('이 날짜에 일정이 없습니다.'),
          );
        }
        
        return ListView.builder(
          itemCount: events.length,
          itemBuilder: (context, index) {
            final event = events[index];
            
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
            
            return Card(
              margin: const EdgeInsets.symmetric(horizontal: 8.0, vertical: 4.0),
              child: ListTile(
                leading: Container(
                  width: 12,
                  height: double.infinity,
                  color: priorityColor,
                ),
                title: Text(event.title),
                subtitle: Text(
                  event.description.isNotEmpty 
                    ? event.description 
                    : (event.isAllDay 
                      ? '종일 일정' 
                      : '${_formatTime(event.startTime)} - ${_formatTime(event.endTime)}'),
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
      },
    );
  }

  String _formatTime(DateTime? time) {
    if (time == null) return '';
    return DateFormat('HH:mm').format(time);
  }

  void _navigateToEventScreen(Event? event) async {
    final result = await Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) => EventScreen(
          selectedDate: _selectedDay,
          event: event,
        ),
      ),
    );
    
    // 새로운 이벤트가 추가되었거나 기존 이벤트가 수정되었을 경우
    if (result == true) {
      Provider.of<EventProvider>(context, listen: false).refreshEvents();
    }
  }

  void _deleteEvent(Event event) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('일정 삭제'),
        content: const Text('이 일정을 삭제하시겠습니까?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('취소'),
          ),
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              Provider.of<EventProvider>(context, listen: false).deleteEvent(event);
            },
            child: const Text('삭제', style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );
  }
}