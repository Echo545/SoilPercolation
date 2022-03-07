import requests

# FILENAME = "testpage.htm"
FILENAME = "graph.htm"
URL = "http://192.168.4.1/edit"


# Uploads a file to the root of the SD card
with open(FILENAME, 'rb') as f:
    r = requests.post(URL, files={FILENAME: f})

    print(r.text)
