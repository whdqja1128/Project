import 'package:flutter/material.dart';

class RepeatSelector extends StatelessWidget {
  final bool isRepeating;
  final String repeatType;
  final ValueChanged<bool> onRepeatingChanged;
  final ValueChanged<String> onRepeatTypeChanged;

  const RepeatSelector({
    super.key,
    required this.isRepeating,
    required this.repeatType,
    required this.onRepeatingChanged,
    required this.onRepeatTypeChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // 반복 설정 스위치
        SwitchListTile(
          title: const Text('반복 일정'),
          value: isRepeating,
          onChanged: onRepeatingChanged,
        ),
        
        // 반복 타입 선택 (반복 옵션이 켜져 있을 때만 표시)
        if (isRepeating)
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16.0, vertical: 8.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  '반복 주기',
                  style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
                ),
                const SizedBox(height: 8),
                Row(
                  children: [
                    _buildRepeatOption(context, 'weekly', '매주', Icons.calendar_view_week),
                    const SizedBox(width: 8),
                    _buildRepeatOption(context, 'monthly', '매월', Icons.calendar_view_month),
                    const SizedBox(width: 8),
                    _buildRepeatOption(context, 'yearly', '매년', Icons.calendar_today),
                  ],
                ),
              ],
            ),
          ),
      ],
    );
  }

  Widget _buildRepeatOption(
    BuildContext context,
    String type,
    String label,
    IconData icon,
  ) {
    final isSelected = repeatType == type;

    return Expanded(
      child: InkWell(
        onTap: () => onRepeatTypeChanged(type),
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 12.0),
          decoration: BoxDecoration(
            color: isSelected ? Colors.blue.withOpacity(0.2) : Colors.transparent,
            border: Border.all(
              color: isSelected ? Colors.blue : Colors.grey,
              width: isSelected ? 2 : 1,
            ),
            borderRadius: BorderRadius.circular(4),
          ),
          child: Column(
            children: [
              Icon(
                icon,
                color: isSelected ? Colors.blue : Colors.grey,
              ),
              const SizedBox(height: 4),
              Text(
                label,
                style: TextStyle(
                  color: isSelected ? Colors.blue : Colors.grey,
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}