import * as React from 'react';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemIcon from '@mui/material/ListItemIcon';
import ListItemText from '@mui/material/ListItemText';
import ListSubheader from '@mui/material/ListSubheader';
import BookIcon from '@mui/icons-material/Book';
import HistoryIcon from '@mui/icons-material/History';
import PowerSettingsNewIcon from '@mui/icons-material/PowerSettingsNew';
import HomeIcon from '@mui/icons-material/Home';
import PersonIcon from '@mui/icons-material/Person';
import LayersIcon from '@mui/icons-material/Layers';
import SettingsIcon from '@mui/icons-material/Settings';
import AssignmentIcon from '@mui/icons-material/Assignment';
import { Link } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import { AxiosError } from 'axios';


export const MainListItems = () => {
  const axiosPrivate = useAxiosPrivate();
  const logout = async () => {
    try {
      const response = await axiosPrivate.get('/api/logout');
      location.reload();
    } catch (err: unknown) {
      if (err instanceof AxiosError) {

      }
    }
  }

  return (
    <React.Fragment>
      <ListItemButton component={Link} to={`/home`}>
        <ListItemIcon>
          <HomeIcon />
        </ListItemIcon>
        <ListItemText primary="Home" />
      </ListItemButton>
      {/* <ListItemButton>
        <ListItemIcon>
          <BookIcon />
        </ListItemIcon>
        <ListItemText primary="Libraries" />
      </ListItemButton>
      <ListItemButton>
        <ListItemIcon>
          <HistoryIcon />
        </ListItemIcon>
        <ListItemText primary="History" />
      </ListItemButton>
      <ListItemButton>
        <ListItemIcon>
          <SettingsIcon />
        </ListItemIcon>
        <ListItemText primary="Server Settings" />
      </ListItemButton>
      <ListItemButton>
        <ListItemIcon>
          <PersonIcon />
        </ListItemIcon>
        <ListItemText primary="Account Settings" />
      </ListItemButton> */}
      <ListItemButton onClick={logout}>
        <ListItemIcon>
          <PowerSettingsNewIcon />
        </ListItemIcon>
        <ListItemText primary="Log Out" />
      </ListItemButton>
    </React.Fragment>
  )
};

export const secondaryListItems = (
  <React.Fragment>
  </React.Fragment>
);
