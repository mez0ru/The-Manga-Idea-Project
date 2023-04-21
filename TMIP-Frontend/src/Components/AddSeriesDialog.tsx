import { useState, useRef, forwardRef, useEffect } from 'react';
import Button from '@mui/material/Button';
import TextField from '@mui/material/TextField';
import Dialog from '@mui/material/Dialog';
import DialogActions from '@mui/material/DialogActions';
import DialogContent from '@mui/material/DialogContent';
import DialogContentText from '@mui/material/DialogContentText';
import DialogTitle from '@mui/material/DialogTitle';
import { TransitionProps } from '@mui/material/transitions';
import Zoom from '@mui/material/Zoom';
import LinearProgress from '@mui/material/LinearProgress';
import useAxiosPrivate from '../hooks/useAxiosPrivate';
import { AxiosError } from 'axios';
import Alert from '@mui/material/Alert';
import UpdatedInfoDialog, { CodeItemInfo } from './UpdatedInfoDialog';
import { Chapter } from '../ChaptersPage';

interface Props {
    open: boolean;
    setOpen: React.Dispatch<React.SetStateAction<boolean>>;
    setInvalidate: React.Dispatch<React.SetStateAction<boolean>>;
}

const Transition = forwardRef(function Transition(
    props: TransitionProps & {
        children: React.ReactElement<any, any>;
    },
    ref: React.Ref<unknown>,
) {
    return <Zoom ref={ref} {...props} />;
});

export default function AddSeries({ open, setOpen, setInvalidate }: Props) {

    //   const handleClickOpen = () => {
    //     setOpen(true);
    //   };

    const [name, setName] = useState('')
    const [folder, setFolder] = useState('');
    const [isLoading, setIsLoading] = useState(false);
    const axiosPrivate = useAxiosPrivate();
    const [errMsg, setErrMsg] = useState('')
    const [showInfo, setShowInfo] = useState(false);
    const [addedChapters, setAddedChapters] = useState<Chapter[]>([])

    const handleClose = () => {
        setIsLoading(false);
        setOpen(false);
    };

    const handleSubmit = async () => {
        setIsLoading(true);
        setErrMsg('');

        try {
            const response = await axiosPrivate.post(`/api/v2/series`, { name, folder_path: folder });
            setAddedChapters(response.data.chapters as Chapter[]);
            handleClose();
            setInvalidate(true);
            setShowInfo(true);
        } catch (err: unknown) {
            if (err instanceof AxiosError) {
                console.log(err);
                setErrMsg(err.response?.data?.error)
            }
            setIsLoading(false);
        }
    }


    return (<>
        <Dialog open={open} onClose={handleClose} TransitionComponent={Transition} keepMounted>
            {isLoading ? <LinearProgress variant='query' /> : null}
            <DialogTitle>Add new Series</DialogTitle>
            <DialogContent>
                {errMsg ? <Alert severity="error">{errMsg}</Alert> : null}

                {/* <DialogContentText>
            To subscribe to this website, please enter your email address here. We
            will send updates occasionally.
          </DialogContentText> */}
                <TextField
                    autoFocus
                    margin="dense"
                    label="Name"
                    type="text"
                    fullWidth
                    variant="standard"
                    required
                    autoComplete='off'
                    value={name}
                    error={errMsg.includes('same name') ? true : false}
                    onChange={(e) => setName(e.currentTarget.value)}
                />
                <TextField
                    margin="dense"
                    label="Folder Path"
                    type="text"
                    fullWidth
                    variant="standard"
                    required
                    autoComplete='off'
                    value={folder}
                    error={errMsg.includes('Folder path') ? true : false}
                    onChange={(e) => setFolder(e.currentTarget.value)}
                />
            </DialogContent>
            <DialogActions>
                <Button onClick={handleClose}>Cancel</Button>
                <Button onClick={handleSubmit} disabled={folder && name && !isLoading ? false : true}>Create</Button>
            </DialogActions>
        </Dialog>
        <UpdatedInfoDialog open={showInfo} setOpen={setShowInfo} title="Action Finished" message={
            <CodeItemInfo elevation={3}>
                Series "{name}" has been added with chapters:<br />
                {addedChapters.map((x: Chapter, index: number) =>
                    <>{`Chapter ${index + 1}: ${x.file_name ?? 'Not added'}`}<br />{`Pages: ${x.pages ?? 0}`}{x.error ? <><br /><p style={{ color: 'red' }}>{`Error: ${x.error}`}</p></> : null}<br /><br /></>)}
            </CodeItemInfo>
        } />
    </>
    );
}