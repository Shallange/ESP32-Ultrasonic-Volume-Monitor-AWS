from discord_webhook import DiscordWebhook, DiscordEmbed
import boto3
import json
from botocore.exceptions import ClientError
from datetime import datetime, timedelta
import dateutil.parser

# Timestream configuration
DATABASE_NAME = "SensorTelementry"
TABLE_NAME = "iot_data"

# Initialize Timestream client
timestream_query = boto3.client('timestream-query')

# Discord Webhook URL
WEBHOOK_URL = "<Insert discord webbhook>"

def lambda_handler(event, context):
    try:
        print("Starting process...")
        # Adjust the query to select the most recent timestamp along with the measurement
        query = f"""
            SELECT measure_name, measure_value::double, time
            FROM "{DATABASE_NAME}"."{TABLE_NAME}"
            WHERE device_id = 'A842E3CDFEB8'
            ORDER BY time DESC LIMIT 1
        """
        result = timestream_query.query(QueryString=query)
        print("Query Result:", result)

        if result['Rows']:
            # Assuming the first row contains the data we need
            row = result['Rows'][0]
            measure_name = row['Data'][0].get('ScalarValue', 'No Measure Name')
            measure_value = row['Data'][1].get('ScalarValue', 'No Measure Value')
            measure_time = row['Data'][2].get('ScalarValue', None)

            if measure_time:
                # Parse the timestamp from Timestream and the current time
                last_measurement_time = dateutil.parser.isoparse(measure_time)
                current_time = datetime.utcnow()
                
                # Define your threshold for considering the device "online"
                threshold_minutes = 5
                if (current_time - last_measurement_time) < timedelta(minutes=threshold_minutes):
                    message = f"The device is currently online. Last measurement: {measure_value} at {last_measurement_time}"
                else:
                    message = "The device is currently offline."
                
                send_discord_message(message)
            else:
                print("No valid timestamp found for the last measurement.")

        return {
            "statusCode": 200,
            "body": json.dumps({"message": "Process completed."})
        }
    except ClientError as e:
        print(f"An error occurred: {e}")
        return {
            "statusCode": 500,
            "body": json.dumps({"message": "Error processing request"})
        }


def get_field_value(field):
    if 'ScalarValue' in field:
        return field['ScalarValue']
    elif 'NullValue' in field and field['NullValue']:
        return None
    else:
        return None  # or appropriate default value


def send_discord_message(message):
    try:
        webhook = DiscordWebhook(url=WEBHOOK_URL, content=message)
        embed = DiscordEmbed(title="Device Status Update", description=message, color="03b2f8")
        webhook.add_embed(embed)

        response = webhook.execute()
        
        # Check if the response has a status code property (indicating a response object)
        if hasattr(response, 'status_code'):
            print("Webhook response status code:", response.status_code)
            if response.status_code != 200:
                print("Failed to send message to Discord:", response.content)
        else:
            print("No status code returned from webhook execution. Possible rate limiting.")

        print("Sent message to Discord:", message)
    except Exception as e:
        print("An error occurred while sending message to Discord:", str(e))



# For local testing
if __name__ == "__main__":
    lambda_handler(None, None)
