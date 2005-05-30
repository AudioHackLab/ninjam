#include <windows.h>

#include "usercon.h"
#include "../mpb.h"


User_Connection::User_Connection(JNL_Connection *con) : m_clientcaps(0), m_auth_state(0)
{
  m_netcon.attach(con);

  // fucko: randomize m_challenge

  mpb_server_auth_challenge ch;
  memcpy(ch.challenge,m_challenge,sizeof(ch.challenge));

  ch.server_caps=0;

  m_netcon.Send(ch.build());
}

User_Connection::~User_Connection()
{
}


void User_Connection::SendConfigChangeNotify(int bpm, int bpi)
{
  if (m_auth_state > 0)
  {
    mpb_server_config_change_notify mk;
    mk.beats_interval=bpi;
    mk.beats_minute=bpm;
    m_netcon.Send(mk.build());
  }
}

int User_Connection::Run(User_Group *group)
{
  if (m_netcon.GetStatus()) return m_netcon.GetStatus();

  Net_Message *msg=m_netcon.Run();
  if (msg)
  {
    if (m_auth_state < 1)
    {
      mpb_client_auth_user authrep;
      if (msg->get_type() != MESSAGE_CLIENT_AUTH_USER || authrep.parse(msg) || !authrep.username || !authrep.username[0])
      {
        m_netcon.Kill();
        return -1;
      }
      // fucko: verify user/password hash

      {
        mpb_server_auth_reply bh;
        bh.flag=1;
        m_netcon.Send(bh.build());
      }

      m_clientcaps=authrep.client_caps;
      m_auth_state=1;

      SendConfigChangeNotify(group->m_last_bpm,group->m_last_bpi);

      // send user list to user
      {
        mpb_server_userinfo_change_notify bh;

        int user;
        for (user = 0; user < group->m_users.GetSize(); user++)
        {
          User_Connection *u=group->m_users.Get(user);
          int channel;
          if (u && u->m_auth_state>0) for (channel = 0; channel < MAX_USER_CHANNELS; channel ++)
          {
            if (u->m_channels[channel].active)
            {
              bh.build_add_rec(1,channel,u->m_channels[channel].volume,u->m_channels[channel].panning,u->m_channels[channel].flags,
                                u->m_username.Get(),u->m_channels[channel].name.Get());
            }
          }
        }       
        m_netcon.Send(bh.build());
      }
    } // m_auth_state < 1


    switch (msg->get_type())
    {
      case MESSAGE_CLIENT_SET_CHANNEL_INFO:
        {
          mpb_client_set_channel_info chi;
          if (!chi.parse(msg))
          {
            // update our channel list

            mpb_server_userinfo_change_notify mfmt;
            int mfmt_changes=0;
            
            int offs=0;
            int v,p,f;
            int whichch=0;
            char *chnp=0;
            while ((offs=chi.parse_get_rec(offs,&chnp,&v,&p,&f)) && whichch < MAX_USER_CHANNELS)
            {
              if (!chnp) chnp=""; 
              // only if something changes, do we add it to the rec
              int hadch=!m_channels[whichch].active;
              if (!hadch) hadch = strcmp(chnp,m_channels[whichch].name.Get());
              if (!hadch) hadch = m_channels[whichch].volume!=v;
              if (!hadch) hadch = m_channels[whichch].panning!=p;
              if (!hadch) hadch = m_channels[whichch].flags!=f;

              m_channels[whichch].active=1;
              m_channels[whichch].name.Set(chnp);
              m_channels[whichch].volume=v;
              m_channels[whichch].panning=p;
              m_channels[whichch].flags=f;

              if (hadch)
              {
                mfmt_changes++;
                mfmt.build_add_rec(m_channels[whichch].active,whichch,
                                  m_channels[whichch].volume,
                                  m_channels[whichch].panning,
                                  m_channels[whichch].flags,
                                  m_username.Get(),
                                  m_channels[whichch].name.Get());
              }

              whichch++;
            }
            while (whichch < MAX_USER_CHANNELS)
            {
              m_channels[whichch].name.Set("");

              if (m_channels[whichch].active) // only send deactivate if it was previously active
              {
                m_channels[whichch].active=0;
                mfmt_changes++;
                mfmt.build_add_rec(0,whichch,
                                  m_channels[whichch].volume,
                                  m_channels[whichch].panning,
                                  m_channels[whichch].flags,
                                  m_username.Get(),
                                  m_channels[whichch].name.Get());
              }

              whichch++;
            }

            if (mfmt_changes) group->Broadcast(mfmt.build(),this);
          }         
        }
      break;

      default:
      break;
    }
  }


  return 0;
}


User_Group::User_Group() : m_last_bpm(120), m_last_bpi(32)
{
}

User_Group::~User_Group()
{
  int x;
  for (x = 0; x < m_users.GetSize(); x ++)
  {
    delete m_users.Get(x);
  }
  m_users.Empty();
}


void User_Group::Broadcast(Net_Message *msg, User_Connection *nosend)
{
  if (msg)
  {
    msg->addRef(); // we do this so that if nobody actually gets to send it, we delete it

    int x;
    for (x = 0; x < m_users.GetSize(); x ++)
    {
      User_Connection *p=m_users.Get(x);
      if (p && p->m_auth_state > 0 && p != nosend)
      {
        p->Send(msg);
      }
    }

    msg->releaseRef();
  }
}

void User_Group::Run()
{
    int x;
    for (x = 0; x < m_users.GetSize(); x ++)
    {
      User_Connection *p=m_users.Get(x);
      if (p)
      {
        if (p->Run(this))
        {
          delete p;
          m_users.Delete(x--);
        }
      }
    }
}

void User_Group::SetConfig(int bpi, int bpm)
{
  m_last_bpi=bpi;
  m_last_bpm=bpm;
  mpb_server_config_change_notify mk;
  mk.beats_interval=bpi;
  mk.beats_minute=bpm;
  Broadcast(mk.build());
}

void User_Group::AddConnection(JNL_Connection *con)
{
  m_users.Add(new User_Connection(con));
}