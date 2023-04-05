import { useEffect, useState, useRef } from 'react'
import { useLocation, useNavigate, useParams } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Grid from '@mui/material/Grid';
import ChapterCard from './ChapterCard';

export interface Chapter {
    id: number;
    name: string;
}

export default function ChaptersPage() {
    let { id } = useParams();


    const navigate = useNavigate();
    const location = useLocation();
    const axiosPrivate = useAxiosPrivate();

    const [chapters, setChapters] = useState<Chapter[]>([])

    useEffect(() => {
        let isMounted = true;
        const controller = new AbortController();

        const getChapters = async () => {
            try {
                const response = await axiosPrivate.get(`/api/series/info/${id}`, {
                    signal: controller.signal
                });
                console.log(response.data);
                isMounted && setChapters(response.data as Chapter[]);
            } catch (err) {
                console.error(err);
                // navigate('/login', { state: { from: location }, replace: true });
            }
        }

        getChapters();

        return () => {
            isMounted = false;
            controller.abort();
        }
    }, [])

    return (<div>
        <Grid container spacing={2} direction="row">
            {
                chapters.map((item, i) => (
                    <Grid item key={i}>
                        <ChapterCard chapter={item} />
                    </Grid>))
            }
        </Grid>
    </div>)
}
