[Profile]
DisplayName = Google Talk
IconName = general_gtalk
Manager = gabble
Protocol = jabber
Priority = -40
VCardDefault = 0
VCardField = X-JABBER
Capabilities = chat-p2p, , , chat-room, chat-room-list, registration-ui, supports-avatars, supports-alias, supports-roster
DefaultAccountDomain = gmail.com, googlemail.com
Default-server = talk.google.com
Default-port = 5223
Default-old-ssl = 1
Default-fallback-conference-server = conference.jabber.org
SupportedPresences = away,extended-away,do-not-disturb
SupportsPresenceMessage = true

[KeepAlive]
ParamName = keepalive-interval
Value-WLAN_INFRA = 120
Value-GPRS = 600

[Presence available]
Name = pres_bd_gtalk_available
IconName = general_presence_online
Type = 2

[Presence busy]
Name = pres_bd_gtalk_busy
IconName = general_presence_busy
Type = 6

[Presence away]
Name = pres_bd_gtalk_away
IconName = general_presence_away
Type = 3

[Presence offline]
Name = pres_bd_gtalk_signout
IconName = general_presence_offline
Type = 1

[Action chat]
Name = addr_bd_cont_starter_im_service_chat
IconName = general_sms
VCardFields = X-JABBER
prop-org.freedesktop.Telepathy.Channel.ChannelType-s = org.freedesktop.Telepathy.Channel.Type.Text

[Action call]
Name = addr_bd_cont_starter_im_service_call
IconName = general_call
VCardFields = X-JABBER
prop-org.freedesktop.Telepathy.Channel.ChannelType-s = org.freedesktop.Telepathy.Channel.Type.StreamedMedia

