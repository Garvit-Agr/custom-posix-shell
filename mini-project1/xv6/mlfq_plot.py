import sys
import re
try:
    import matplotlib.pyplot as plt
except ModuleNotFoundError:
    print("Error: matplotlib is not installed. Please install it using 'pip3 install matplotlib' or 'sudo apt install python3-matplotlib'.")
    sys.exit(1)
username="garvit.agrawal@students.iiit.ac.in"
def generate_plot():
    pid_data={}
    start_tick=None
    with open("mlfq.txt","r") as f:
        for line in f:
            if line.startswith("TRACE:"):
                parts=line.strip().split()
                if len(parts)>=4:
                    tick=int(parts[1])
                    pid=int(parts[2])
                    queue=int(parts[3])
                    if start_tick is None:
                        start_tick=tick
                    normalized_tick=tick-start_tick
                    if pid not in pid_data:
                        pid_data[pid]={'x':[],'y':[]}
                    pid_data[pid]['x'].append(normalized_tick)
                    pid_data[pid]['y'].append(queue)
    plt.figure(figsize=(12,6))
    colors=['r','g','b','c','m','y','k']
    color_idx=0
    process_num=1
    for pid in sorted(pid_data.keys()):
        if pid>3:
            jitter = (color_idx - 2) * 0.05
            jittered_y = [y + jitter for y in pid_data[pid]['y']]
            plt.scatter(pid_data[pid]['x'],jittered_y,label=f'Process {process_num}',s=15,color=colors[color_idx%len(colors)])
            plt.plot(pid_data[pid]['x'],jittered_y,alpha=0.5,linewidth=1.5,color=colors[color_idx%len(colors)], drawstyle='steps-post')
            color_idx+=1
            process_num+=1
    plt.yticks([0,1,2,3],['Queue 0','Queue 1','Queue 2','Queue 3'])
    plt.gca().invert_yaxis()
    plt.xlabel('Time (Ticks)')
    plt.ylabel('Queue ID')
    plt.title('MLFQ Timeline Analysis')
    plt.legend(loc='lower right')
    plt.grid(True,linestyle='--',alpha=0.5)
    plt.text(0.95,0.95,username,ha='right',va='top',transform=plt.gca().transAxes,fontsize=10,color="gray",alpha=0.7)
    plt.tight_layout()
    plt.savefig("mlfq_plot.png",dpi=300)
    print("Saved plot to mlfq_plot.png")
if __name__=="__main__":
    generate_plot()
