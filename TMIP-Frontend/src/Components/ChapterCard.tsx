import { useState, useEffect, useRef } from 'react'
import { BASE_URL } from '../api/axios';
import useAxiosPrivate from '../hooks/useAxiosPrivate';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Typography from '@mui/material/Typography';
import CardActionArea from '@mui/material/CardActionArea';
import { Chapter } from '../ChaptersPage';
import Grow from '@mui/material/Grow';
import { Link } from 'react-router-dom';

interface Props {
    chapter: Chapter;
}

export default function SeriesCard({ chapter }: Props) {
    const axiosPrivate = useAxiosPrivate();
    const imageRef = useRef<HTMLImageElement>(null);
    const [done, setDone] = useState(false);

    useEffect(() => {
        let objectUrl: string
        const ev = () => URL.revokeObjectURL(objectUrl);

        const imageOnLoad = async () => {
            // Fetch the image.
            try {
                const response = await axiosPrivate.get(`${BASE_URL}/api/chapter/${chapter.id}/cover`, { responseType: 'blob' });

                // Create an object URL from the data.
                const blob = response.data;
                objectUrl = URL.createObjectURL(blob);

                // Update the source of the image.
                imageRef.current!!.src = objectUrl;
            } catch (err: unknown) {

            }
            imageRef.current?.addEventListener('load', ev);
            setDone(true);
        }
        imageOnLoad();

        return () => {
            imageRef.current?.removeEventListener('load', ev);
            setDone(false);
        }
    }, [])


    return (
        <Grow in={done}>
            <Card sx={{ maxWidth: 150 }}>
                <CardActionArea component={Link} to={`/series/${chapter.seriesId}/chapter/${chapter.id}`}>
                    <CardMedia
                        component="img"
                        alt={chapter.name}
                        ref={imageRef}
                    />
                    <CardContent>
                        <Typography gutterBottom variant="body1" sx={{
                            overflow: "hidden",
                            textOverflow: "ellipsis",
                            display: "-webkit-box",
                            WebkitLineClamp: "2",
                            WebkitBoxOrient: "vertical",
                        }}>
                            {chapter.name}
                        </Typography>
                        <br />
                        <Typography gutterBottom variant="body2">
                            {chapter.pages} Pages
                        </Typography>
                    </CardContent>
                </CardActionArea>
            </Card>
        </Grow>
    )
}
