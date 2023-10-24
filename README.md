# What is this? 
This library makes it easy to interact with an AWS s3 bucket from an ESP32.
The ESP32 and its family of devices (ESP32-S2, S3, C3, etc.) are well known for their ability to connect to a Wi-Fi connection and are therefore good candidates for IOT type projects.
Often there is a need to store and retrieve data from a remote location. 
This library helps you use AWS S3 buckets with a http client to upload and download files. You can use this to 

*	upload logs, error reports, etc. 
*	Download configuration, OTA (Over the Air [Firmware]) updates. Etc.

Advantages of using AWS
*	Eliminate the need to setup, run and maintain extra infrastructure, web servers, admin portals etc.
*	S3 storage is inexpensive.
*	Files can be automatically versioned.
*	Fine grain security control
*	AWS can be setup to trigger lambda functions to add data to databases, send emails or anything else you can imagine.
