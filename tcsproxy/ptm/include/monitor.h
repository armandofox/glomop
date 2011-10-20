#ifndef __MONITOR_H__
#define __MONITOR_H__


#ifdef __cplusplus
extern "C"
{
#endif


typedef void *MonitorClientID;

/*
 * Create a client object for the monitoring GUI
 * Returns the ID of the object. Returns 0 on failure
 * -- for now the monitoring client only supports multicast
 * -- will add unicast when possible
 */
MonitorClientID
MonitorClientCreate(const char *unitID, const char *multicastAddr, 
		    Port port, int ttl);

/*
 * Destroy a monitor client object
 */
void MonitorClientDestroy(MonitorClientID client);

const char *
MonitorClientUnitID(MonitorClientID client);

/*
 * Send a message to the monitor
 * script can be one of the following:
 *    "" - do the default i.e. print the fieldID, latest sender and value on
 *         a separate line
 *    "Log" - log this message inside a log window
 *    "proc new_unit { toplevel unit } { ... }\n 
 *     proc new_field { frame unit field } { ... }\n
 *     proc update_field { frame unit field value sender } { ... }\n"
 *         - the above 3 line script can be used to define arbitrary
 *           scripts to create new fields and update them
 */
gm_Bool
MonitorClientSend(MonitorClientID client, const char *fieldID, 
		  const char *value, const char *script);

/*
 * Same as above, but can be used to send a message with a unitID that is
 * different from the default one set in MonitorClientCreate
 */
gm_Bool
MonitorClientSend_With_Unit(MonitorClientID client, const char *unitID, 
			    const char *fieldID, const char *value, 
			    const char *script);


#ifdef __cplusplus
}
#endif


#endif /* __MONITOR_H__ */
