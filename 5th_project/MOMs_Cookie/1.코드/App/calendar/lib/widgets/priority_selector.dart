import 'package:flutter/material.dart';

class PrioritySelector extends StatelessWidget {
  final int selectedPriority;
  final ValueChanged<int> onPriorityChanged;

  const PrioritySelector({
    super.key,
    required this.selectedPriority,
    required this.onPriorityChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          '중요도',
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            _buildPriorityOption(context, 1, '낮음', Colors.green),
            const SizedBox(width: 8),
            _buildPriorityOption(context, 2, '중간', Colors.orange),
            const SizedBox(width: 8),
            _buildPriorityOption(context, 3, '높음', Colors.red),
          ],
        ),
      ],
    );
  }

  Widget _buildPriorityOption(
    BuildContext context,
    int priority,
    String label,
    Color color,
  ) {
    final isSelected = selectedPriority == priority;

    return Expanded(
      child: InkWell(
        onTap: () => onPriorityChanged(priority),
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 12.0),
          decoration: BoxDecoration(
            color: isSelected ? color.withOpacity(0.2) : Colors.transparent,
            border: Border.all(
              color: isSelected ? color : Colors.grey,
              width: isSelected ? 2 : 1,
            ),
            borderRadius: BorderRadius.circular(4),
          ),
          child: Column(
            children: [
              Icon(
                _getPriorityIcon(priority),
                color: isSelected ? color : Colors.grey,
              ),
              const SizedBox(height: 4),
              Text(
                label,
                style: TextStyle(
                  color: isSelected ? color : Colors.grey,
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  IconData _getPriorityIcon(int priority) {
    switch (priority) {
      case 1:
        return Icons.low_priority;
      case 2:
        return Icons.priority_high;
      case 3:
        return Icons.warning_rounded;
      default:
        return Icons.help_outline;
    }
  }
}