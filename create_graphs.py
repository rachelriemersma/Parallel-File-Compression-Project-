# create_graphs.py
import matplotlib.pyplot as plt
import numpy as np

# Set style
plt.style.use('seaborn-v0_8-darkgrid')
colors = ['#2E86AB', '#A23B72', '#F18F01', '#C73E1D', '#6A994E']

# Create output directory
import os
os.makedirs('graphs', exist_ok=True)

# Graph 1: Thread Scaling - Speedup (70MB file)
threads = [1, 2, 4, 8, 16, 32]
speedup = [1.00, 2.39, 3.91, 7.14, 17.58, 14.75]
ideal_speedup = threads  # Perfect linear scaling

fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(threads, speedup, 'o-', linewidth=2, markersize=10, label='Actual Speedup', color=colors[0])
ax.plot(threads, ideal_speedup, '--', linewidth=2, label='Ideal (Linear)', color='gray', alpha=0.5)
ax.set_xlabel('Number of Threads', fontsize=14, fontweight='bold')
ax.set_ylabel('Speedup', fontsize=14, fontweight='bold')
ax.set_title('Thread Scaling Performance (70MB File)', fontsize=16, fontweight='bold')
ax.legend(fontsize=12)
ax.grid(True, alpha=0.3)
ax.set_xticks(threads)
plt.tight_layout()
plt.savefig('graphs/thread_speedup.png', dpi=300, bbox_inches='tight')
plt.close()

# Graph 2: Thread Scaling - Efficiency
efficiency = [100.0, 119.5, 97.75, 89.25, 109.875, 46.09]

fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(threads, efficiency, 'o-', linewidth=2, markersize=10, color=colors[1])
ax.axhline(y=100, color='gray', linestyle='--', linewidth=2, alpha=0.5, label='100% Efficiency')
ax.set_xlabel('Number of Threads', fontsize=14, fontweight='bold')
ax.set_ylabel('Efficiency (%)', fontsize=14, fontweight='bold')
ax.set_title('Parallel Efficiency vs Thread Count', fontsize=16, fontweight='bold')
ax.legend(fontsize=12)
ax.grid(True, alpha=0.3)
ax.set_xticks(threads)
ax.set_ylim([0, 130])
plt.tight_layout()
plt.savefig('graphs/thread_efficiency.png', dpi=300, bbox_inches='tight')
plt.close()

# Graph 3: Block Size Comparison
block_sizes = [100, 300, 500, 900, 2000, 5000]
throughput = [160.86, 129.05, 128.39, 106.76, 60.78, 62.61]

fig, ax = plt.subplots(figsize=(10, 6))
bars = ax.bar(range(len(block_sizes)), throughput, color=colors[2], alpha=0.8, edgecolor='black')
ax.set_xlabel('Block Size (KB)', fontsize=14, fontweight='bold')
ax.set_ylabel('Throughput (MB/s)', fontsize=14, fontweight='bold')
ax.set_title('Impact of Block Size on Performance', fontsize=16, fontweight='bold')
ax.set_xticks(range(len(block_sizes)))
ax.set_xticklabels(block_sizes)
ax.grid(True, alpha=0.3, axis='y')

# Add value labels on bars
for i, bar in enumerate(bars):
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2., height,
            f'{throughput[i]:.1f}',
            ha='center', va='bottom', fontsize=10, fontweight='bold')

plt.tight_layout()
plt.savefig('graphs/block_size_comparison.png', dpi=300, bbox_inches='tight')
plt.close()

# Graph 4: File Size Scaling
file_sizes = [6, 30, 61, 124, 313]
file_throughput = [20.97, 63.25, 107.37, 111.25, 107.63]

fig, ax = plt.subplots(figsize=(10, 6))
ax.plot(file_sizes, file_throughput, 'o-', linewidth=2, markersize=10, color=colors[3])
ax.set_xlabel('File Size (MB)', fontsize=14, fontweight='bold')
ax.set_ylabel('Throughput (MB/s)', fontsize=14, fontweight='bold')
ax.set_title('Throughput vs File Size (16 threads)', fontsize=16, fontweight='bold')
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig('graphs/file_size_scaling.png', dpi=300, bbox_inches='tight')
plt.close()

# Graph 5: Memory Optimization Comparison
categories = ['Before\nOptimization', 'After\nOptimization']
memory_usage = [391, 121]
memory_colors = [colors[4], '#90BE6D']

fig, ax = plt.subplots(figsize=(8, 6))
bars = ax.bar(categories, memory_usage, color=memory_colors, alpha=0.8, edgecolor='black', linewidth=2)
ax.set_ylabel('Memory Usage (MB)', fontsize=14, fontweight='bold')
ax.set_title('Memory Optimization Results (281MB File)', fontsize=16, fontweight='bold')
ax.grid(True, alpha=0.3, axis='y')

# Add value labels and savings
for i, bar in enumerate(bars):
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2., height,
            f'{memory_usage[i]} MB',
            ha='center', va='bottom', fontsize=12, fontweight='bold')

# Add savings annotation
ax.annotate('69% Reduction',
            xy=(0.5, 250), xytext=(0.8, 300),
            fontsize=14, fontweight='bold', color='green',
            arrowprops=dict(arrowstyle='->', color='green', lw=2))

plt.tight_layout()
plt.savefig('graphs/memory_optimization.png', dpi=300, bbox_inches='tight')
plt.close()

print("All graphs created successfully in 'graphs/' directory!")
print("\nGenerated files:")
print("1. thread_speedup.png")
print("2. thread_efficiency.png")
print("3. block_size_comparison.png")
print("4. file_size_scaling.png")
print("5. memory_optimization.png")